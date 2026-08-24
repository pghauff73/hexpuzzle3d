#include "puzzle.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace hexpuzzle {
namespace {

Imath::V3f normalized(Imath::V3f value) {
    if (value.length() != 0.0f) {
        value.normalize();
    }
    return value;
}

bool strictlyBetween(
    std::size_t start,
    std::size_t value,
    std::size_t end,
    std::size_t sideCount) {
    const std::size_t span = (end + sideCount - start) % sideCount;
    const std::size_t offset = (value + sideCount - start) % sideCount;
    return offset != 0 && offset < span;
}

bool pathsCross(
    const ConnectorPath& first,
    const ConnectorPath& second,
    std::size_t sideCount) {
    const bool firstEndpointInside = strictlyBetween(
        first.firstSide,
        second.firstSide,
        first.secondSide,
        sideCount);
    const bool secondEndpointInside = strictlyBetween(
        first.firstSide,
        second.secondSide,
        first.secondSide,
        sideCount);
    return firstEndpointInside != secondEndpointInside;
}

bool pathsAreNoncrossing(
    const std::vector<ConnectorPath>& paths,
    std::size_t sideCount) {
    for (std::size_t first = 0; first < paths.size(); ++first) {
        for (std::size_t second = first + 1; second < paths.size(); ++second) {
            if (pathsCross(paths[first], paths[second], sideCount)) {
                return false;
            }
        }
    }
    return true;
}

std::size_t maximumPathCount(std::size_t sideCount) {
    return std::min<std::size_t>(3, sideCount / 2);
}

std::size_t pathSeparation(const ConnectorPath& path, std::size_t sideCount) {
    const std::size_t difference =
        (path.secondSide + sideCount - path.firstSide) % sideCount;
    return std::min(difference, sideCount - difference);
}

std::size_t layoutMaximumSpan(const ConnectorLayout& layout, std::size_t sideCount) {
    std::size_t result = 0;
    for (const ConnectorPath& path : layout) {
        result = std::max(result, pathSeparation(path, sideCount));
    }
    return result;
}

void enumerateConnectorLayouts(
    std::size_t sideCount,
    std::vector<bool>& consumed,
    ConnectorLayout& current,
    std::vector<ConnectorLayout>& result) {
    const auto firstAvailable = std::find(consumed.begin(), consumed.end(), false);
    if (firstAvailable == consumed.end()) {
        if (!current.empty() && pathsAreNoncrossing(current, sideCount)) {
            result.push_back(current);
        }
        return;
    }

    const std::size_t firstSide =
        static_cast<std::size_t>(firstAvailable - consumed.begin());
    consumed[firstSide] = true;
    enumerateConnectorLayouts(sideCount, consumed, current, result);

    if (current.size() < maximumPathCount(sideCount)) {
        for (std::size_t secondSide = firstSide + 1; secondSide < sideCount; ++secondSide) {
            if (consumed[secondSide]) {
                continue;
            }
            const ConnectorPath candidate{firstSide, secondSide};
            bool crosses = false;
            for (const ConnectorPath& path : current) {
                crosses = crosses || pathsCross(path, candidate, sideCount);
            }
            if (crosses) {
                continue;
            }
            consumed[secondSide] = true;
            current.push_back(candidate);
            enumerateConnectorLayouts(sideCount, consumed, current, result);
            current.pop_back();
            consumed[secondSide] = false;
        }
    }
    consumed[firstSide] = false;
}

bool layoutLess(const ConnectorLayout& left, const ConnectorLayout& right) {
    if (left.size() != right.size()) {
        return left.size() < right.size();
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].firstSide != right[index].firstSide) {
            return left[index].firstSide < right[index].firstSide;
        }
        if (left[index].secondSide != right[index].secondSide) {
            return left[index].secondSide < right[index].secondSide;
        }
    }
    return false;
}

std::vector<ConnectorLayout> buildConnectorLayoutCatalog(std::size_t sideCount) {
    if (sideCount != 5 && sideCount != 6) {
        throw std::invalid_argument("connector catalogs require five or six sides");
    }
    std::vector<bool> consumed(sideCount, false);
    ConnectorLayout current;
    std::vector<ConnectorLayout> result;
    enumerateConnectorLayouts(sideCount, consumed, current, result);
    std::sort(result.begin(), result.end(), layoutLess);
    return result;
}

std::size_t uniformIndex(std::mt19937& random, std::size_t count) {
    if (count == 0) {
        throw std::invalid_argument("cannot choose from an empty range");
    }
    const std::uint64_t range =
        static_cast<std::uint64_t>(std::mt19937::max()) + 1ULL;
    const std::uint64_t limit = range - range % count;
    std::uint64_t value = 0;
    do {
        value = random();
    } while (value >= limit);
    return static_cast<std::size_t>(value % count);
}

bool metricsBetter(const BoardMetrics& candidate, const BoardMetrics& current) {
    return std::tie(
               candidate.qualityScore,
               candidate.longestRoute,
               candidate.connectedEdges,
               candidate.connectedPaths) >
            std::tie(
               current.qualityScore,
               current.longestRoute,
               current.connectedEdges,
               current.connectedPaths) ||
        (candidate.qualityScore == current.qualityScore &&
         candidate.longestRoute == current.longestRoute &&
         candidate.connectedEdges == current.connectedEdges &&
         candidate.connectedPaths == current.connectedPaths &&
         std::tie(candidate.isolatedPaths, candidate.routeComponents) <
             std::tie(current.isolatedPaths, current.routeComponents));
}

}  // namespace

const std::vector<ConnectorLayout>& connectorLayoutCatalog(std::size_t sideCount) {
    static const std::vector<ConnectorLayout> pentagonCatalog =
        buildConnectorLayoutCatalog(5);
    static const std::vector<ConnectorLayout> hexagonCatalog =
        buildConnectorLayoutCatalog(6);
    if (sideCount == 5) {
        return pentagonCatalog;
    }
    if (sideCount == 6) {
        return hexagonCatalog;
    }
    throw std::invalid_argument("connector catalogs require five or six sides");
}

PuzzleTile::PuzzleTile(
    std::size_t id,
    const Imath::V3f& center,
    std::vector<std::size_t> neighbors)
    : id_(id), center_(normalized(center)), neighbors_(std::move(neighbors)) {
    if (neighbors_.size() < 5 || neighbors_.size() > 6) {
        throw std::invalid_argument("puzzle tiles must have five or six sides");
    }
}

std::size_t PuzzleTile::id() const noexcept {
    return id_;
}

const Imath::V3f& PuzzleTile::center() const noexcept {
    return center_;
}

const std::vector<std::size_t>& PuzzleTile::neighbors() const noexcept {
    return neighbors_;
}

std::size_t PuzzleTile::sideCount() const noexcept {
    return neighbors_.size();
}

std::size_t PuzzleTile::neighbor(std::size_t side) const {
    return neighbors_.at(side);
}

const std::vector<ConnectorPath>& PuzzleTile::paths() const noexcept {
    return paths_;
}

std::size_t PuzzleTile::pathCount() const noexcept {
    return paths_.size();
}

std::size_t PuzzleTile::rotation() const noexcept {
    return rotation_;
}

bool PuzzleTile::active() const noexcept {
    return active_;
}

void PuzzleTile::randomize(std::mt19937& random) {
    const std::vector<ConnectorLayout>& catalog = connectorLayoutCatalog(sideCount());
    const std::size_t targetPathCount = 1 + uniformIndex(random, maximumPathCount(sideCount()));
    std::array<bool, 4> availableSpans{};
    for (const ConnectorLayout& layout : catalog) {
        if (layout.size() == targetPathCount) {
            availableSpans[layoutMaximumSpan(layout, sideCount())] = true;
        }
    }
    std::vector<std::size_t> spans;
    for (std::size_t span = 1; span < availableSpans.size(); ++span) {
        if (availableSpans[span]) {
            spans.push_back(span);
        }
    }
    const std::size_t targetSpan = spans.at(uniformIndex(random, spans.size()));
    std::vector<std::size_t> eligibleLayouts;
    for (std::size_t index = 0; index < catalog.size(); ++index) {
        if (catalog[index].size() == targetPathCount &&
            layoutMaximumSpan(catalog[index], sideCount()) == targetSpan) {
            eligibleLayouts.push_back(index);
        }
    }
    paths_ = catalog.at(eligibleLayouts.at(uniformIndex(random, eligibleLayouts.size())));
    rotation_ = 0;
}

void PuzzleTile::rotate() {
    for (ConnectorPath& path : paths_) {
        path.firstSide = (path.firstSide + 1) % sideCount();
        path.secondSide = (path.secondSide + 1) % sideCount();
    }
    rotation_ = (rotation_ + 1) % sideCount();
}

bool PuzzleTile::hasPort(std::size_t side) const {
    return pairedSide(side).has_value();
}

std::optional<std::size_t> PuzzleTile::pairedSide(std::size_t side) const noexcept {
    const std::optional<std::size_t> pathIndex = pathIndexForSide(side);
    if (!pathIndex.has_value()) {
        return std::nullopt;
    }
    const ConnectorPath& path = paths_[*pathIndex];
    return path.firstSide == side ? path.secondSide : path.firstSide;
}

std::optional<std::size_t> PuzzleTile::pathIndexForSide(std::size_t side) const noexcept {
    if (side >= sideCount()) {
        return std::nullopt;
    }
    for (std::size_t index = 0; index < paths_.size(); ++index) {
        if (paths_[index].firstSide == side || paths_[index].secondSide == side) {
            return index;
        }
    }
    return std::nullopt;
}

void PuzzleTile::setActive(bool active) noexcept {
    active_ = active;
}

PuzzleBoard::PuzzleBoard(
    const HexPlanet& planet,
    std::uint32_t randomSeed,
    PuzzleGenerationSettings generationSettings)
    : random_(randomSeed), generationSettings_(generationSettings) {
    if (generationSettings_.candidateCount == 0) {
        throw std::invalid_argument("puzzle generation requires at least one candidate board");
    }
    buildTraversal(planet);
    initializeTiles(planet);
    generateBoardCandidates();
}

std::size_t PuzzleBoard::tileCount() const noexcept {
    return tiles_.size();
}

const PuzzleTile& PuzzleBoard::tile(std::size_t id) const {
    return tiles_.at(id);
}

PuzzleTile& PuzzleBoard::tile(std::size_t id) {
    return tiles_.at(id);
}

const std::vector<std::size_t>& PuzzleBoard::traversalPath() const noexcept {
    return traversalPath_;
}

void PuzzleBoard::rotateTile(std::size_t id) {
    tile(id).rotate();
    updateConnections();
}

void PuzzleBoard::updateConnections() {
    for (auto& puzzleTile : tiles_) {
        puzzleTile.setActive(false);
    }
    for (auto& puzzleTile : tiles_) {
        for (std::size_t side = 0; side < puzzleTile.sideCount(); ++side) {
            PuzzleTile& adjacent = tile(puzzleTile.neighbor(side));
            if (isConnected(puzzleTile.id(), side)) {
                puzzleTile.setActive(true);
                adjacent.setActive(true);
            }
        }
    }
    metrics_ = calculateMetrics();
}

std::vector<std::size_t> PuzzleBoard::connectedSides(std::size_t id) const {
    const PuzzleTile& selected = tile(id);
    std::vector<std::size_t> result;
    for (std::size_t side = 0; side < selected.sideCount(); ++side) {
        if (isConnected(id, side)) {
            result.push_back(side);
        }
    }
    return result;
}

std::vector<ConnectedRoute> PuzzleBoard::connectedRoutes(std::size_t startId) const {
    const PuzzleTile& start = tile(startId);
    std::vector<std::vector<bool>> visited;
    visited.reserve(tileCount());
    for (const PuzzleTile& puzzleTile : tiles_) {
        visited.emplace_back(puzzleTile.pathCount(), false);
    }

    std::vector<ConnectedRoute> routes;
    for (std::size_t startPath = 0; startPath < start.pathCount(); ++startPath) {
        if (visited[startId][startPath]) {
            continue;
        }
        routes.push_back(collectRoute({startId, startPath}, visited));
    }
    return routes;
}

std::vector<ConnectedRoute> PuzzleBoard::allRoutes() const {
    std::vector<std::vector<bool>> visited;
    visited.reserve(tileCount());
    for (const PuzzleTile& puzzleTile : tiles_) {
        visited.emplace_back(puzzleTile.pathCount(), false);
    }

    std::vector<ConnectedRoute> routes;
    for (const PuzzleTile& puzzleTile : tiles_) {
        for (std::size_t pathIndex = 0; pathIndex < puzzleTile.pathCount(); ++pathIndex) {
            if (!visited[puzzleTile.id()][pathIndex]) {
                routes.push_back(collectRoute({puzzleTile.id(), pathIndex}, visited));
            }
        }
    }
    return routes;
}

const BoardMetrics& PuzzleBoard::metrics() const noexcept {
    return metrics_;
}

ConnectedRoute PuzzleBoard::collectRoute(
    RouteSegment start,
    std::vector<std::vector<bool>>& visited) const {
    ConnectedRoute route;
    std::vector<RouteSegment> pending{start};
    visited[start.tileId][start.pathIndex] = true;

    while (!pending.empty()) {
        const RouteSegment segment = pending.back();
        pending.pop_back();
        route.push_back(segment);
        const PuzzleTile& current = tile(segment.tileId);
        const ConnectorPath& path = current.paths().at(segment.pathIndex);
        const std::size_t pathSides[2]{path.firstSide, path.secondSide};
        for (const std::size_t side : pathSides) {
            if (!isConnected(segment.tileId, side)) {
                continue;
            }
            const std::size_t adjacentId = current.neighbor(side);
            const std::size_t adjacentSide = reciprocalSide(segment.tileId, side);
            const std::optional<std::size_t> adjacentPath =
                tile(adjacentId).pathIndexForSide(adjacentSide);
            if (!adjacentPath.has_value()) {
                throw std::logic_error("connected side is not owned by an adjacent path");
            }
            if (!visited[adjacentId][*adjacentPath]) {
                visited[adjacentId][*adjacentPath] = true;
                pending.push_back({adjacentId, *adjacentPath});
            }
        }
    }
    return route;
}

BoardMetrics PuzzleBoard::calculateMetrics() const {
    BoardMetrics result;
    for (const PuzzleTile& puzzleTile : tiles_) {
        result.totalPaths += puzzleTile.pathCount();
        for (const ConnectorPath& path : puzzleTile.paths()) {
            const bool connected =
                isConnected(puzzleTile.id(), path.firstSide) ||
                isConnected(puzzleTile.id(), path.secondSide);
            result.connectedPaths += connected ? 1 : 0;
            result.isolatedPaths += connected ? 0 : 1;
        }
        for (std::size_t side = 0; side < puzzleTile.sideCount(); ++side) {
            if (puzzleTile.id() < puzzleTile.neighbor(side) &&
                isConnected(puzzleTile.id(), side)) {
                ++result.connectedEdges;
            }
        }
    }

    const std::vector<ConnectedRoute> routes = allRoutes();
    result.routeComponents = routes.size();
    for (const ConnectedRoute& route : routes) {
        result.longestRoute = std::max(result.longestRoute, route.size());
    }
    result.qualityScore =
        static_cast<std::int64_t>(result.connectedEdges) * 64 +
        static_cast<std::int64_t>(result.connectedPaths) * 12 +
        static_cast<std::int64_t>(result.longestRoute) * 24 -
        static_cast<std::int64_t>(result.isolatedPaths) * 16 -
        static_cast<std::int64_t>(result.routeComponents) * 2;
    return result;
}

void PuzzleBoard::generateBoardCandidates() {
    const std::vector<PuzzleTile> emptyTiles = tiles_;
    std::vector<PuzzleTile> bestTiles;
    BoardMetrics bestMetrics;
    bool hasBest = false;

    for (std::size_t candidate = 0; candidate < generationSettings_.candidateCount; ++candidate) {
        tiles_ = emptyTiles;
        for (PuzzleTile& puzzleTile : tiles_) {
            puzzleTile.randomize(random_);
        }
        updateConnections();
        if (!hasBest || metricsBetter(metrics_, bestMetrics)) {
            bestTiles = tiles_;
            bestMetrics = metrics_;
            hasBest = true;
        }
    }
    tiles_ = std::move(bestTiles);
    metrics_ = bestMetrics;
}

std::size_t PuzzleBoard::reciprocalSide(std::size_t id, std::size_t side) const {
    const PuzzleTile& selected = tile(id);
    const PuzzleTile& adjacent = tile(selected.neighbor(side));
    const auto reciprocal = std::find(adjacent.neighbors().begin(), adjacent.neighbors().end(), id);
    if (reciprocal == adjacent.neighbors().end()) {
        throw std::logic_error("puzzle topology contains a non-reciprocal neighbor");
    }
    return static_cast<std::size_t>(reciprocal - adjacent.neighbors().begin());
}

bool PuzzleBoard::isConnected(std::size_t id, std::size_t side) const {
    const PuzzleTile& selected = tile(id);
    if (side >= selected.sideCount() || !selected.hasPort(side)) {
        return false;
    }
    const PuzzleTile& adjacent = tile(selected.neighbor(side));
    return adjacent.hasPort(reciprocalSide(id, side));
}

void PuzzleBoard::buildTraversal(const HexPlanet& planet) {
    traversalPath_.clear();
    traversalPath_.reserve(planet.tileCount());
    std::vector<bool> visited(planet.tileCount(), false);
    if (planet.tileCount() != 0) {
        visit(0, planet, visited);
    }
    if (traversalPath_.size() != planet.tileCount()) {
        throw std::runtime_error("planet graph traversal did not reach every tile");
    }
}

void PuzzleBoard::visit(
    std::size_t id,
    const HexPlanet& planet,
    std::vector<bool>& visited) {
    visited[id] = true;
    traversalPath_.push_back(id);
    auto adjacent = planet.neighbors(id);
    std::shuffle(adjacent.begin(), adjacent.end(), random_);
    for (const std::size_t next : adjacent) {
        if (!visited[next]) {
            visit(next, planet, visited);
        }
    }
}

void PuzzleBoard::initializeTiles(const HexPlanet& planet) {
    tiles_.clear();
    tiles_.reserve(planet.tileCount());
    for (std::size_t id = 0; id < planet.tileCount(); ++id) {
        tiles_.emplace_back(id, planet.tile(id).normal, planet.neighbors(id));
    }
}

}  // namespace hexpuzzle
