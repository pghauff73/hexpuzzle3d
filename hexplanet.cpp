#include "hexplanet.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace hexpuzzle {
namespace {

constexpr float pi = 3.14159265358979323846f;

Imath::V3f normalized(Imath::V3f value) {
    if (value.length() != 0.0f) {
        value.normalize();
    }
    return value;
}

Imath::V3f tangentComponent(const Imath::V3f& direction, const Imath::V3f& normal) {
    return normalized(direction - normal * direction.dot(normal));
}

bool rayTriangleDistance(
    const Ray& ray,
    const Imath::V3f& first,
    const Imath::V3f& second,
    const Imath::V3f& third,
    float& distance) {
    constexpr float epsilon = 0.000001f;
    const Imath::V3f firstEdge = second - first;
    const Imath::V3f secondEdge = third - first;
    const Imath::V3f perpendicular = ray.direction.cross(secondEdge);
    const float determinant = firstEdge.dot(perpendicular);
    if (std::abs(determinant) < epsilon) {
        return false;
    }
    const float inverseDeterminant = 1.0f / determinant;
    const Imath::V3f fromFirst = ray.origin - first;
    const float firstCoordinate = fromFirst.dot(perpendicular) * inverseDeterminant;
    if (firstCoordinate < 0.0f || firstCoordinate > 1.0f) {
        return false;
    }
    const Imath::V3f secondPerpendicular = fromFirst.cross(firstEdge);
    const float secondCoordinate = ray.direction.dot(secondPerpendicular) * inverseDeterminant;
    if (secondCoordinate < 0.0f || firstCoordinate + secondCoordinate > 1.0f) {
        return false;
    }
    distance = secondEdge.dot(secondPerpendicular) * inverseDeterminant;
    return distance >= 0.0f;
}

using Edge = std::pair<std::size_t, std::size_t>;

Edge canonicalEdge(std::size_t first, std::size_t second) {
    return {std::min(first, second), std::max(first, second)};
}

std::pair<std::size_t, std::size_t> triangleEdge(
    const HexTriangle& triangle,
    std::size_t edgeIndex) {
    switch (edgeIndex) {
        case 0:
            return {triangle.vertices[0], triangle.vertices[1]};
        case 1:
            return {triangle.vertices[1], triangle.vertices[2]};
        default:
            return {triangle.vertices[2], triangle.vertices[0]};
    }
}

}  // namespace

HexPlanet::HexPlanet()
    : HexPlanet(Settings{}) {
}

HexPlanet::HexPlanet(Settings settings)
    : settings_(settings), random_(settings.randomSeed) {
    if (settings_.subdivisionLevel < 0) {
        throw std::invalid_argument("subdivision level cannot be negative");
    }
    if (settings_.terrainRandomness < 0.0f || settings_.terrainRandomness > 1.0f) {
        throw std::invalid_argument("terrain randomness must be between zero and one");
    }
    if (settings_.waterProbability < 0.0f || settings_.waterProbability > 1.0f) {
        throw std::invalid_argument("water probability must be between zero and one");
    }

    buildLevelZero();
    while (subdivisionLevel_ < settings_.subdivisionLevel) {
        subdivide();
    }
    if (settings_.subdivisionLevel == 0) {
        projectToSphere();
        rebuildConnectivity();
    }
}

int HexPlanet::subdivisionLevel() const noexcept {
    return subdivisionLevel_;
}

std::size_t HexPlanet::tileCount() const noexcept {
    return tiles_.size();
}

std::size_t HexPlanet::triangleCount() const noexcept {
    return triangles_.size();
}

const HexTile& HexPlanet::tile(std::size_t index) const {
    return tiles_.at(index);
}

const HexTriangle& HexPlanet::triangle(std::size_t index) const {
    return triangles_.at(index);
}

const std::vector<HexTile>& HexPlanet::tiles() const noexcept {
    return tiles_;
}

const std::vector<HexTriangle>& HexPlanet::triangles() const noexcept {
    return triangles_;
}

void HexPlanet::buildLevelZero() {
    tiles_.clear();
    triangles_.clear();

    const std::array<Imath::V3f, 12> vertices{
        Imath::V3f(0.723606f, 0.0f, 1.17082f),
        Imath::V3f(0.0f, 1.17082f, 0.723606f),
        Imath::V3f(-0.723606f, 0.0f, 1.17082f),
        Imath::V3f(0.0f, -1.17082f, 0.723606f),
        Imath::V3f(0.723606f, 0.0f, -1.17082f),
        Imath::V3f(0.0f, -1.17082f, -0.723606f),
        Imath::V3f(-0.723606f, 0.0f, -1.17082f),
        Imath::V3f(0.0f, 1.17082f, -0.723606f),
        Imath::V3f(1.17082f, -0.723606f, 0.0f),
        Imath::V3f(1.17082f, 0.723606f, 0.0f),
        Imath::V3f(-1.17082f, 0.723606f, 0.0f),
        Imath::V3f(-1.17082f, -0.723606f, 0.0f),
    };
    tiles_.reserve(vertices.size());
    for (const auto& position : vertices) {
        tiles_.push_back({position, normalized(position), randomTerrain(), {}});
    }

    const std::array<std::array<std::size_t, 3>, 20> faces{
        std::array<std::size_t, 3>{5, 11, 6},
        {1, 2, 0},
        {0, 2, 3},
        {5, 6, 4},
        {4, 6, 7},
        {9, 1, 0},
        {10, 2, 1},
        {2, 10, 11},
        {11, 3, 2},
        {8, 9, 0},
        {0, 3, 8},
        {11, 10, 6},
        {4, 7, 9},
        {9, 8, 4},
        {7, 6, 10},
        {1, 9, 7},
        {10, 1, 7},
        {8, 3, 5},
        {5, 4, 8},
        {3, 11, 5},
    };
    triangles_.reserve(faces.size());
    for (const auto& face : faces) {
        triangles_.push_back({face});
    }
    rebuildConnectivity();
}

void HexPlanet::subdivide() {
    for (auto& triangle : triangles_) {
        triangle.subdivisionVertex = tiles_.size();
        const auto position = triangleCenter(triangle);
        TerrainType terrain = randomTerrain();
        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
        if (chance(random_) > settings_.terrainRandomness) {
            std::uniform_int_distribution<int> parent(0, 2);
            terrain = tiles_[triangle.vertices[static_cast<std::size_t>(parent(random_))]].terrain;
        }
        tiles_.push_back({position, normalized(position), terrain, {}});
    }

    std::vector<HexTriangle> nextTriangles;
    nextTriangles.reserve(triangles_.size() * 3);
    std::map<Edge, bool> processed;
    for (std::size_t triangleIndex = 0; triangleIndex < triangles_.size(); ++triangleIndex) {
        const auto& triangle = triangles_[triangleIndex];
        for (std::size_t edgeIndex = 0; edgeIndex < 3; ++edgeIndex) {
            const auto [edgeStart, edgeEnd] = triangleEdge(triangle, edgeIndex);
            const Edge edge = canonicalEdge(edgeStart, edgeEnd);
            if (processed[edge]) {
                continue;
            }
            const std::size_t neighborIndex = triangle.neighbors[edgeIndex];
            if (neighborIndex == HexTriangle::invalidIndex) {
                throw std::runtime_error("planet mesh contains an open edge");
            }
            const std::size_t center = triangle.subdivisionVertex;
            const std::size_t neighborCenter = triangles_[neighborIndex].subdivisionVertex;
            nextTriangles.push_back({{edgeStart, center, neighborCenter}});
            nextTriangles.push_back({{center, neighborCenter, edgeEnd}});
            processed[edge] = true;
        }
    }

    triangles_ = std::move(nextTriangles);
    ++subdivisionLevel_;
    projectToSphere();
    rebuildConnectivity();
}

void HexPlanet::rebuildConnectivity() {
    for (auto& tile : tiles_) {
        tile.incidentTriangles.clear();
    }
    for (auto& triangle : triangles_) {
        triangle.neighbors.fill(HexTriangle::invalidIndex);
    }

    std::map<Edge, std::pair<std::size_t, std::size_t>> firstEdge;
    for (std::size_t triangleIndex = 0; triangleIndex < triangles_.size(); ++triangleIndex) {
        auto& triangle = triangles_[triangleIndex];
        for (const std::size_t vertex : triangle.vertices) {
            tiles_.at(vertex).incidentTriangles.push_back(triangleIndex);
        }
        for (std::size_t edgeIndex = 0; edgeIndex < 3; ++edgeIndex) {
            const auto [edgeStart, edgeEnd] = triangleEdge(triangle, edgeIndex);
            const Edge edge = canonicalEdge(edgeStart, edgeEnd);
            const auto found = firstEdge.find(edge);
            if (found == firstEdge.end()) {
                firstEdge.emplace(edge, std::make_pair(triangleIndex, edgeIndex));
                continue;
            }
            const auto [otherTriangle, otherEdge] = found->second;
            triangle.neighbors[edgeIndex] = otherTriangle;
            triangles_[otherTriangle].neighbors[otherEdge] = triangleIndex;
        }
    }

    for (auto& tile : tiles_) {
        if (tile.incidentTriangles.size() < 3) {
            throw std::runtime_error("planet tile has insufficient incident triangles");
        }
        const Imath::V3f normal = normalized(tile.position);
        const Imath::V3f firstDirection = normalized(
            triangleCenter(triangles_[tile.incidentTriangles.front()]) - tile.position);
        Imath::V3f tangent = firstDirection - normal * firstDirection.dot(normal);
        tangent = normalized(tangent);
        const Imath::V3f bitangent = normalized(normal.cross(tangent));
        std::sort(
            tile.incidentTriangles.begin(),
            tile.incidentTriangles.end(),
            [&](std::size_t left, std::size_t right) {
                const auto angleFor = [&](std::size_t triangleIndex) {
                    Imath::V3f direction = triangleCenter(triangles_[triangleIndex]) - tile.position;
                    direction = normalized(direction - normal * direction.dot(normal));
                    float angle = std::atan2(direction.dot(bitangent), direction.dot(tangent));
                    if (angle < 0.0f) {
                        angle += 2.0f * pi;
                    }
                    return angle;
                };
                return angleFor(left) < angleFor(right);
            });
    }
}

void HexPlanet::projectToSphere() {
    for (auto& tile : tiles_) {
        tile.normal = normalized(tile.position);
        tile.position = tile.normal * radius;
    }
}

TerrainType HexPlanet::randomTerrain() {
    std::uniform_real_distribution<float> chance(0.0f, 1.0f);
    if (chance(random_) <= settings_.waterProbability) {
        return TerrainType::Water;
    }
    std::uniform_int_distribution<int> land(
        static_cast<int>(TerrainType::Desert),
        static_cast<int>(TerrainType::Mountain));
    return static_cast<TerrainType>(land(random_));
}

Imath::V3f HexPlanet::triangleCenter(const HexTriangle& triangle) const {
    return (
        tiles_[triangle.vertices[0]].position +
        tiles_[triangle.vertices[1]].position +
        tiles_[triangle.vertices[2]].position) /
        3.0f;
}

std::vector<Imath::V3f> HexPlanet::polygon(
    std::size_t tileIndex,
    SurfaceOffset offset) const {
    const auto& selectedTile = tiles_.at(tileIndex);
    std::vector<Imath::V3f> result;
    result.reserve(selectedTile.incidentTriangles.size());
    for (const std::size_t triangleIndex : selectedTile.incidentTriangles) {
        result.push_back(
            normalized(triangleCenter(triangles_[triangleIndex])) * (radius + offset.value));
    }
    return result;
}

Imath::V3f HexPlanet::sideAnchor(
    std::size_t tileIndex,
    std::size_t side,
    SurfaceOffset offset,
    float centerInset) const {
    if (centerInset < 0.0f || centerInset >= 1.0f) {
        throw std::invalid_argument("side anchor inset must be in [0, 1)");
    }
    const std::vector<Imath::V3f> tilePolygon = polygon(tileIndex, offset);
    if (side >= tilePolygon.size()) {
        throw std::out_of_range("side anchor is outside the tile polygon");
    }
    const float surfaceRadius = radius + offset.value;
    const Imath::V3f edgeCenter =
        (tilePolygon[side] + tilePolygon[(side + 1) % tilePolygon.size()]) * 0.5f;
    const Imath::V3f tileCenter = tile(tileIndex).normal * surfaceRadius;
    return normalized(edgeCenter * (1.0f - centerInset) + tileCenter * centerInset) *
        surfaceRadius;
}

SideConnectionCurve HexPlanet::sideConnectionCurve(
    std::size_t tileIndex,
    std::size_t firstSide,
    std::size_t secondSide,
    SurfaceOffset offset,
    float centerInset,
    std::size_t segmentCount) const {
    if (firstSide == secondSide) {
        throw std::invalid_argument("a side connection requires two distinct sides");
    }
    if (segmentCount < 2) {
        throw std::invalid_argument("a side connection curve requires at least two segments");
    }

    const std::vector<Imath::V3f> tilePolygon = polygon(tileIndex, offset);
    if (firstSide >= tilePolygon.size() || secondSide >= tilePolygon.size()) {
        throw std::out_of_range("side connection endpoint is outside the tile polygon");
    }

    const float surfaceRadius = radius + offset.value;
    const Imath::V3f tileCenter = tile(tileIndex).normal * surfaceRadius;
    const auto inwardNormal = [&](std::size_t side, const Imath::V3f& anchor) {
        const Imath::V3f surfaceNormal = normalized(anchor);
        const Imath::V3f edgeDirection = tangentComponent(
            tilePolygon[(side + 1) % tilePolygon.size()] - tilePolygon[side],
            surfaceNormal);
        Imath::V3f inward = normalized(surfaceNormal.cross(edgeDirection));
        const Imath::V3f towardCenter = tangentComponent(tileCenter - anchor, surfaceNormal);
        if (inward.dot(towardCenter) < 0.0f) {
            inward = -inward;
        }
        return inward;
    };

    const Imath::V3f first = sideAnchor(tileIndex, firstSide, offset, centerInset);
    const Imath::V3f second = sideAnchor(tileIndex, secondSide, offset, centerInset);
    const Imath::V3f firstInward = inwardNormal(firstSide, first);
    const Imath::V3f secondInward = inwardNormal(secondSide, second);
    const Imath::V3f firstTravel = tangentComponent(second - first, normalized(first));
    const Imath::V3f secondTravel = tangentComponent(first - second, normalized(second));

    SideConnectionCurve result;
    result.straight =
        firstInward.dot(firstTravel) >= 0.9995f &&
        secondInward.dot(secondTravel) >= 0.9995f;
    result.points.reserve(segmentCount + 1);

    const float handleLength = (second - first).length() * 0.22f;
    const Imath::V3f firstControl = first + firstInward * handleLength;
    const Imath::V3f secondControl = second + secondInward * handleLength;
    for (std::size_t segment = 0; segment <= segmentCount; ++segment) {
        const float amount = static_cast<float>(segment) / static_cast<float>(segmentCount);
        Imath::V3f point;
        if (result.straight) {
            point = first * (1.0f - amount) + second * amount;
        } else {
            const float remaining = 1.0f - amount;
            point =
                first * (remaining * remaining * remaining) +
                firstControl * (3.0f * remaining * remaining * amount) +
                secondControl * (3.0f * remaining * amount * amount) +
                second * (amount * amount * amount);
        }
        result.points.push_back(normalized(point) * surfaceRadius);
    }
    result.points.front() = first;
    result.points.back() = second;
    return result;
}

std::vector<std::size_t> HexPlanet::neighbors(std::size_t tileIndex) const {
    const auto& selectedTile = tiles_.at(tileIndex);
    std::vector<std::size_t> result;
    result.reserve(selectedTile.incidentTriangles.size());
    for (std::size_t edge = 0; edge < selectedTile.incidentTriangles.size(); ++edge) {
        const HexTriangle& first = triangles_.at(selectedTile.incidentTriangles[edge]);
        const HexTriangle& second = triangles_.at(
            selectedTile.incidentTriangles[(edge + 1) % selectedTile.incidentTriangles.size()]);
        const auto sharedNeighbor = std::find_if(
            first.vertices.begin(),
            first.vertices.end(),
            [&](std::size_t vertex) {
                return vertex != tileIndex &&
                    std::find(second.vertices.begin(), second.vertices.end(), vertex) != second.vertices.end();
            });
        if (sharedNeighbor == first.vertices.end()) {
            throw std::logic_error("adjacent tile triangles do not share an edge");
        }
        if (std::find(result.begin(), result.end(), *sharedNeighbor) != result.end()) {
            throw std::logic_error("tile polygon maps multiple edges to one neighbor");
        }
        result.push_back(*sharedNeighbor);
    }
    return result;
}

std::size_t HexPlanet::nearestTile(const Imath::V3f& surfaceDirection) const {
    if (tiles_.empty()) {
        throw std::runtime_error("cannot select a tile on an empty planet");
    }
    const Imath::V3f direction = normalized(surfaceDirection);
    std::size_t bestIndex = 0;
    float bestDot = tiles_.front().normal.dot(direction);
    for (std::size_t index = 1; index < tiles_.size(); ++index) {
        const float candidate = tiles_[index].normal.dot(direction);
        if (candidate > bestDot) {
            bestDot = candidate;
            bestIndex = index;
        }
    }
    return bestIndex;
}

bool HexPlanet::rayIntersection(const Ray& input, Imath::V3f& result) const {
    const Imath::V3f direction = normalized(input.direction);
    const float a = direction.dot(direction);
    const float b = 2.0f * direction.dot(input.origin);
    const float c = input.origin.dot(input.origin) - radius * radius;
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return false;
    }
    const float root = std::sqrt(discriminant);
    const float nearDistance = (-b - root) / (2.0f * a);
    const float farDistance = (-b + root) / (2.0f * a);
    const float distance = nearDistance >= 0.0f ? nearDistance : farDistance;
    if (distance < 0.0f) {
        return false;
    }
    result = input.origin + direction * distance;
    return true;
}

bool HexPlanet::tileIntersection(
    const Ray& input,
    std::size_t& tileIndex,
    Imath::V3f& result,
    SurfaceOffset offset) const {
    const Ray ray{input.origin, normalized(input.direction)};
    float closestDistance = std::numeric_limits<float>::max();
    bool found = false;
    for (std::size_t id = 0; id < tiles_.size(); ++id) {
        const std::vector<Imath::V3f> tilePolygon = polygon(id, offset);
        const Imath::V3f center = tiles_[id].normal * (radius + offset.value);
        for (std::size_t edge = 0; edge < tilePolygon.size(); ++edge) {
            float distance = 0.0f;
            if (!rayTriangleDistance(
                    ray,
                    center,
                    tilePolygon[edge],
                    tilePolygon[(edge + 1) % tilePolygon.size()],
                    distance) ||
                distance >= closestDistance) {
                continue;
            }
            closestDistance = distance;
            tileIndex = id;
            found = true;
        }
    }
    if (found) {
        result = ray.origin + ray.direction * closestDistance;
    }
    return found;
}

}  // namespace hexpuzzle
