#include "camera.h"
#include "bounded_adams_bashforth.h"
#include "debug_log.h"
#include "hexplanet.h"
#include "puzzle.h"
#include "sequence_tracker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

Imath::V3d asDouble(const Imath::V3f& value) {
    return {value.x, value.y, value.z};
}

Imath::V3d normalized(Imath::V3d value) {
    if (value.length() != 0.0) {
        value.normalize();
    }
    return value;
}

Imath::V3d tangentComponent(const Imath::V3d& direction, const Imath::V3d& normal) {
    return normalized(direction - normal * direction.dot(normal));
}

struct Point2D {
    double x = 0.0;
    double y = 0.0;
};

double cross2D(const Point2D& first, const Point2D& second, const Point2D& third) {
    return
        (second.x - first.x) * (third.y - first.y) -
        (second.y - first.y) * (third.x - first.x);
}

bool properSegmentsIntersect(
    const Point2D& firstStart,
    const Point2D& firstEnd,
    const Point2D& secondStart,
    const Point2D& secondEnd) {
    constexpr double epsilon = 0.0000001;
    const double firstSide = cross2D(firstStart, firstEnd, secondStart);
    const double secondSide = cross2D(firstStart, firstEnd, secondEnd);
    const double thirdSide = cross2D(secondStart, secondEnd, firstStart);
    const double fourthSide = cross2D(secondStart, secondEnd, firstEnd);
    return
        ((firstSide > epsilon && secondSide < -epsilon) ||
         (firstSide < -epsilon && secondSide > epsilon)) &&
        ((thirdSide > epsilon && fourthSide < -epsilon) ||
         (thirdSide < -epsilon && fourthSide > epsilon));
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

bool connectorPathsCross(
    const hexpuzzle::ConnectorPath& first,
    const hexpuzzle::ConnectorPath& second,
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

std::string connectorLayoutKey(const hexpuzzle::ConnectorLayout& layout) {
    std::string result;
    for (const hexpuzzle::ConnectorPath& path : layout) {
        result += std::to_string(path.firstSide) + "-" + std::to_string(path.secondSide) + ";";
    }
    return result;
}

void requireMetricsConsistent(const hexpuzzle::PuzzleBoard& board) {
    const hexpuzzle::BoardMetrics& metrics = board.metrics();
    const std::vector<hexpuzzle::ConnectedRoute> routes = board.allRoutes();
    std::size_t totalPaths = 0;
    std::size_t connectedPaths = 0;
    std::size_t connectedSideCount = 0;
    std::size_t longestRoute = 0;
    for (std::size_t id = 0; id < board.tileCount(); ++id) {
        const hexpuzzle::PuzzleTile& tile = board.tile(id);
        totalPaths += tile.pathCount();
        const std::vector<std::size_t> connectedSides = board.connectedSides(id);
        connectedSideCount += connectedSides.size();
        for (const hexpuzzle::ConnectorPath& path : tile.paths()) {
            const bool connected =
                std::find(connectedSides.begin(), connectedSides.end(), path.firstSide) != connectedSides.end() ||
                std::find(connectedSides.begin(), connectedSides.end(), path.secondSide) != connectedSides.end();
            connectedPaths += connected ? 1 : 0;
        }
    }
    for (const hexpuzzle::ConnectedRoute& route : routes) {
        longestRoute = std::max(longestRoute, route.size());
    }
    require(metrics.totalPaths == totalPaths, "board metrics must count every tile path");
    require(metrics.connectedPaths == connectedPaths, "board metrics must count externally connected paths");
    require(metrics.isolatedPaths == totalPaths - connectedPaths, "board metrics must count isolated paths");
    require(metrics.connectedEdges == connectedSideCount / 2, "board metrics must count each shared edge once");
    require(metrics.routeComponents == routes.size(), "board metrics must count route components");
    require(metrics.longestRoute == longestRoute, "board metrics must expose the longest route");
}

void testPlanetCountsAndTopology() {
    hexpuzzle::HexPlanet levelZero({0, 0.17f, 0.5f, 7});
    require(levelZero.tileCount() == 12, "level zero must contain 12 tiles");
    require(levelZero.triangleCount() == 20, "level zero must contain 20 triangles");

    hexpuzzle::HexPlanet levelTwo({2, 0.17f, 0.5f, 7});
    require(levelTwo.tileCount() == 92, "level two tile count changed");
    require(levelTwo.triangleCount() == 180, "level two triangle count changed");

    std::size_t pentagons = 0;
    for (std::size_t id = 0; id < levelTwo.tileCount(); ++id) {
        const auto adjacent = levelTwo.neighbors(id);
        const hexpuzzle::HexTile& tile = levelTwo.tile(id);
        require(adjacent.size() == 5 || adjacent.size() == 6, "tiles must be pentagons or hexagons");
        pentagons += adjacent.size() == 5 ? 1 : 0;
        require(levelTwo.polygon(id).size() == adjacent.size(), "polygon and adjacency degrees must match");
        for (std::size_t edge = 0; edge < adjacent.size(); ++edge) {
            const std::size_t neighbor = adjacent[edge];
            const auto reverse = levelTwo.neighbors(neighbor);
            require(
                std::find(reverse.begin(), reverse.end(), id) != reverse.end(),
                "planet adjacency must be reciprocal");

            const hexpuzzle::HexTriangle& first = levelTwo.triangle(tile.incidentTriangles[edge]);
            const hexpuzzle::HexTriangle& second =
                levelTwo.triangle(tile.incidentTriangles[(edge + 1) % tile.incidentTriangles.size()]);
            const auto sharedNeighbor = std::find_if(
                first.vertices.begin(),
                first.vertices.end(),
                [&](std::size_t vertex) {
                    return vertex != id &&
                        std::find(second.vertices.begin(), second.vertices.end(), vertex) != second.vertices.end();
                });
            require(sharedNeighbor != first.vertices.end(), "consecutive polygon vertices must share a tile edge");
            require(*sharedNeighbor == neighbor, "neighbor side index must match its polygon edge index");
        }
    }
    require(pentagons == 12, "an icosphere must retain 12 pentagons");
}

void testSelectionAndRayIntersection() {
    hexpuzzle::HexPlanet planet({1, 0.17f, 0.5f, 11});
    for (std::size_t id = 0; id < planet.tileCount(); ++id) {
        require(planet.nearestTile(planet.tile(id).position) == id, "tile centers must select themselves");
        std::size_t pickedTile = planet.tileCount();
        Imath::V3f tileHit;
        const Imath::V3f direction = planet.tile(id).normal;
        require(
            planet.tileIntersection({direction * 30.0f, -direction}, pickedTile, tileHit),
            "a ray through a rendered tile center must hit the board");
        require(pickedTile == id, "rendered polygon picking must select the tile under the ray");
    }

    Imath::V3f hit;
    require(
        planet.rayIntersection(
            {Imath::V3f(0.0f, 0.0f, 30.0f), Imath::V3f(0.0f, 0.0f, -1.0f)},
            hit),
        "forward ray must hit planet");
    require(std::abs(hit.length() - hexpuzzle::HexPlanet::radius) < 0.001f, "ray hit must lie on sphere");
    require(
        !planet.rayIntersection(
            {Imath::V3f(0.0f, 0.0f, 30.0f), Imath::V3f(0.0f, 1.0f, 0.0f)},
            hit),
        "missed ray must report false");
    std::size_t missedTile = 0;
    require(
        !planet.tileIntersection(
            {Imath::V3f(0.0f, 0.0f, 30.0f), Imath::V3f(0.0f, 1.0f, 0.0f)},
            missedTile,
            hit),
        "a ray outside the rendered board must not select a tile");
}

void testConnectorLayoutCatalogs() {
    const std::array<std::size_t, 7> expectedCatalogSizes{0, 0, 0, 0, 0, 20, 50};
    const std::array<std::array<std::size_t, 4>, 7> expectedPathCounts{{
        {},
        {},
        {},
        {},
        {},
        {0, 10, 10, 0},
        {0, 15, 30, 5},
    }};

    for (const std::size_t sideCount : {std::size_t{5}, std::size_t{6}}) {
        const std::vector<hexpuzzle::ConnectorLayout>& catalog =
            hexpuzzle::connectorLayoutCatalog(sideCount);
        require(catalog.size() == expectedCatalogSizes[sideCount], "noncrossing catalog size changed");
        std::array<std::size_t, 4> pathCounts{};
        std::set<std::string> uniqueLayouts;
        for (const hexpuzzle::ConnectorLayout& layout : catalog) {
            require(!layout.empty(), "connector catalogs must not contain void layouts");
            require(layout.size() <= std::min<std::size_t>(3, sideCount / 2), "catalog path count is invalid");
            ++pathCounts[layout.size()];
            require(uniqueLayouts.insert(connectorLayoutKey(layout)).second, "connector catalogs must be duplicate-free");

            std::vector<bool> usedSides(sideCount, false);
            for (std::size_t first = 0; first < layout.size(); ++first) {
                const hexpuzzle::ConnectorPath& path = layout[first];
                require(path.firstSide < path.secondSide, "catalog paths must use canonical endpoint ordering");
                require(path.secondSide < sideCount, "catalog path endpoints must be valid");
                require(
                    !usedSides[path.firstSide] && !usedSides[path.secondSide],
                    "catalog paths must own exclusive sides");
                usedSides[path.firstSide] = true;
                usedSides[path.secondSide] = true;
                for (std::size_t second = first + 1; second < layout.size(); ++second) {
                    require(
                        !connectorPathsCross(path, layout[second], sideCount),
                        "catalog paths must never cross");
                }
            }
        }
        require(pathCounts == expectedPathCounts[sideCount], "catalog path-count distribution changed");
    }

    bool rejectedInvalidSideCount = false;
    try {
        static_cast<void>(hexpuzzle::connectorLayoutCatalog(4));
    } catch (const std::invalid_argument&) {
        rejectedInvalidSideCount = true;
    }
    require(rejectedInvalidSideCount, "connector catalogs must reject unsupported polygons");
}

void testSideConnectionCurves() {
    hexpuzzle::HexPlanet planet({2, 0.17f, 0.5f, 7});
    std::array<std::optional<std::size_t>, 2> representativeTiles;
    for (std::size_t tileIndex = 0; tileIndex < planet.tileCount(); ++tileIndex) {
        const std::size_t sideCount = planet.polygon(tileIndex).size();
        const std::size_t slot = sideCount == 5 ? 0 : 1;
        if (!representativeTiles[slot].has_value()) {
            representativeTiles[slot] = tileIndex;
        }
    }

    std::size_t curvedPathCount = 0;
    std::size_t straightPathCount = 0;
    constexpr float surfaceOffset = 1.2f;
    constexpr float centerInset = 0.1f;
    constexpr std::size_t segmentCount = 64;
    for (const std::optional<std::size_t> representative : representativeTiles) {
        require(representative.has_value(), "planet must expose representative pentagon and hexagon tiles");
        const std::size_t tileIndex = *representative;
        const std::vector<Imath::V3f> polygon =
            planet.polygon(tileIndex, hexpuzzle::SurfaceOffset{surfaceOffset});
        const Imath::V3d normal = normalized(asDouble(planet.tile(tileIndex).normal));
        const Imath::V3d center = normal * (hexpuzzle::HexPlanet::radius + surfaceOffset);
        const Imath::V3d xAxis = tangentComponent(asDouble(polygon.front()) - center, normal);
        const Imath::V3d yAxis = normalized(normal.cross(xAxis));
        const auto project = [&](const Imath::V3f& point) {
            const Imath::V3d relative = asDouble(point) - center;
            return Point2D{relative.dot(xAxis), relative.dot(yAxis)};
        };

        for (const hexpuzzle::ConnectorLayout& layout :
             hexpuzzle::connectorLayoutCatalog(polygon.size())) {
            std::vector<hexpuzzle::SideConnectionCurve> curves;
            curves.reserve(layout.size());
            for (const hexpuzzle::ConnectorPath& path : layout) {
                const hexpuzzle::SideConnectionCurve curve = planet.sideConnectionCurve(
                    tileIndex,
                    path.firstSide,
                    path.secondSide,
                    hexpuzzle::SurfaceOffset{surfaceOffset},
                    centerInset,
                    segmentCount);
                require(curve.points.size() == segmentCount + 1, "side curve sampling changed");
                curvedPathCount += curve.straight ? 0 : 1;
                straightPathCount += curve.straight ? 1 : 0;

                const std::size_t pathSides[2]{path.firstSide, path.secondSide};
                const std::size_t endpointIndices[2]{0, curve.points.size() - 1};
                for (std::size_t endpoint = 0; endpoint < 2; ++endpoint) {
                    const std::size_t side = pathSides[endpoint];
                    const Imath::V3f expectedAnchor = planet.sideAnchor(
                        tileIndex,
                        side,
                        hexpuzzle::SurfaceOffset{surfaceOffset},
                        centerInset);
                    require(
                        (curve.points[endpointIndices[endpoint]] - expectedAnchor).length() < 0.0001f,
                        "curve endpoints and rendered side anchors must share one geometry");
                    const Imath::V3d endpointNormal =
                        normalized(asDouble(curve.points[endpointIndices[endpoint]]));
                    const Imath::V3d edgeDirection = tangentComponent(
                        asDouble(polygon[(side + 1) % polygon.size()]) - asDouble(polygon[side]),
                        endpointNormal);
                    const Imath::V3d sampledDerivative = endpoint == 0 ?
                        asDouble(curve.points[1]) * 4.0 -
                            asDouble(curve.points[0]) * 3.0 -
                            asDouble(curve.points[2]) :
                        asDouble(curve.points.back()) * 3.0 -
                            asDouble(curve.points[curve.points.size() - 2]) * 4.0 +
                            asDouble(curve.points[curve.points.size() - 3]);
                    const Imath::V3d curveDirection = tangentComponent(
                        sampledDerivative,
                        endpointNormal);
                    require(
                        std::abs(edgeDirection.dot(curveDirection)) < 0.04,
                        "paths must meet entry and exit sides perpendicularly");
                    require(
                        std::abs(curve.points[endpointIndices[endpoint]].length() -
                            (hexpuzzle::HexPlanet::radius + surfaceOffset)) < 0.0001f,
                        "path endpoints must remain on the requested surface");
                }

                if (curve.straight) {
                    const Imath::V3d firstNormal = normalized(asDouble(curve.points.front()));
                    const Imath::V3d secondNormal = normalized(asDouble(curve.points.back()));
                    const Imath::V3d firstDirection = tangentComponent(
                        asDouble(curve.points.back()) - asDouble(curve.points.front()),
                        firstNormal);
                    const Imath::V3d secondDirection = tangentComponent(
                        asDouble(curve.points.front()) - asDouble(curve.points.back()),
                        secondNormal);
                    const Imath::V3d firstEdge = tangentComponent(
                        asDouble(polygon[(path.firstSide + 1) % polygon.size()]) -
                            asDouble(polygon[path.firstSide]),
                        firstNormal);
                    const Imath::V3d secondEdge = tangentComponent(
                        asDouble(polygon[(path.secondSide + 1) % polygon.size()]) -
                            asDouble(polygon[path.secondSide]),
                        secondNormal);
                    require(
                        std::abs(firstDirection.dot(firstEdge)) < 0.04 &&
                            std::abs(secondDirection.dot(secondEdge)) < 0.04,
                        "straight paths are only valid when perpendicular at both sides");
                }
                curves.push_back(curve);
            }

            for (std::size_t first = 0; first < curves.size(); ++first) {
                for (std::size_t second = first + 1; second < curves.size(); ++second) {
                    for (std::size_t firstSegment = 1;
                         firstSegment < curves[first].points.size();
                         ++firstSegment) {
                        for (std::size_t secondSegment = 1;
                             secondSegment < curves[second].points.size();
                             ++secondSegment) {
                            require(
                                !properSegmentsIntersect(
                                    project(curves[first].points[firstSegment - 1]),
                                    project(curves[first].points[firstSegment]),
                                    project(curves[second].points[secondSegment - 1]),
                                    project(curves[second].points[secondSegment])),
                                "concurrent path curves must not cross");
                        }
                    }
                }
            }
        }
    }
    require(curvedPathCount > 0, "non-aligned side pairs must render as curves");
    require(straightPathCount > 0, "aligned opposing sides must retain straight paths");
}

void testPuzzleBoardOwnershipAndDeterminism() {
    hexpuzzle::HexPlanet planet({2, 0.17f, 0.5f, 19});
    hexpuzzle::PuzzleBoard first(planet, 23);
    hexpuzzle::PuzzleBoard second(planet, 23);
    std::array<bool, 4> observedPathCounts{};
    std::array<bool, 4> observedHexSeparations{};
    require(first.tileCount() == planet.tileCount(), "board must own one puzzle tile per planet tile");
    require(first.traversalPath().size() == planet.tileCount(), "board traversal must cover the planet");
    require(
        std::set<std::size_t>(first.traversalPath().begin(), first.traversalPath().end()).size() == planet.tileCount(),
        "board traversal must not repeat tiles");

    for (std::size_t id = 0; id < first.tileCount(); ++id) {
        const hexpuzzle::PuzzleTile& tile = first.tile(id);
        const hexpuzzle::PuzzleTile& matchingTile = second.tile(id);
        require(tile.id() == id, "puzzle tile identity must match vector ownership");
        require(tile.rotation() == matchingTile.rotation(), "equal seeds must reproduce rotations");
        require(tile.pathCount() == matchingTile.pathCount(), "equal seeds must reproduce path counts");
        require(tile.pathCount() >= 1, "every puzzle tile must contain at least one path");
        require(
            tile.pathCount() <= std::min<std::size_t>(3, tile.sideCount() / 2),
            "a tile must contain at most three disjoint side-pair paths");
        observedPathCounts[tile.pathCount()] = true;

        std::vector<bool> usedSides(tile.sideCount(), false);
        for (std::size_t pathIndex = 0; pathIndex < tile.pathCount(); ++pathIndex) {
            const hexpuzzle::ConnectorPath& path = tile.paths()[pathIndex];
            const hexpuzzle::ConnectorPath& matchingPath = matchingTile.paths()[pathIndex];
            require(
                path.firstSide == matchingPath.firstSide && path.secondSide == matchingPath.secondSide,
                "equal seeds must reproduce every side-pair path");
            require(
                path.firstSide < tile.sideCount() && path.secondSide < tile.sideCount(),
                "path endpoints must identify valid tile sides");
            require(path.firstSide != path.secondSide, "a path must enter and leave through different sides");
            require(
                !usedSides[path.firstSide] && !usedSides[path.secondSide],
                "independent paths must not share a side");
            usedSides[path.firstSide] = true;
            usedSides[path.secondSide] = true;
            require(
                tile.pathIndexForSide(path.firstSide) == pathIndex &&
                    tile.pathIndexForSide(path.secondSide) == pathIndex,
                "each occupied side must belong to exactly one path");
            require(
                tile.pairedSide(path.firstSide) == path.secondSide &&
                    tile.pairedSide(path.secondSide) == path.firstSide,
                "each path must be a symmetric binary side connection");
            if (tile.sideCount() == 6) {
                const std::size_t difference =
                    (path.secondSide + tile.sideCount() - path.firstSide) % tile.sideCount();
                observedHexSeparations[std::min(difference, tile.sideCount() - difference)] = true;
            }

            for (std::size_t otherIndex = pathIndex + 1; otherIndex < tile.pathCount(); ++otherIndex) {
                require(
                    !connectorPathsCross(path, tile.paths()[otherIndex], tile.sideCount()),
                    "concurrent paths must not cross inside a tile");
            }
        }

        std::size_t portCount = 0;
        for (std::size_t side = 0; side < tile.sideCount(); ++side) {
            const bool used = usedSides[side];
            require(tile.hasPort(side) == used, "ports must match the generated side-pair paths");
            require(tile.pairedSide(side).has_value() == used, "unused sides must not expose a connection");
            portCount += used ? 1 : 0;
        }
        require(portCount == tile.pathCount() * 2, "every path must own exactly two unique sides");
    }

    for (std::size_t pathCount = 1; pathCount <= 3; ++pathCount) {
        require(observedPathCounts[pathCount], "seeded board must exercise one, two, and three-path tiles");
        require(
            observedHexSeparations[pathCount],
            "seeded hexagons must exercise adjacent, over-one-side, and opposite-side connectors");
    }

    const std::size_t rotatable = 0;
    const std::vector<hexpuzzle::ConnectorPath> previousPaths = first.tile(rotatable).paths();
    const std::size_t previous = first.tile(rotatable).rotation();
    const std::size_t sideCount = first.tile(rotatable).sideCount();
    first.rotateTile(rotatable);
    require(first.tile(rotatable).rotation() != previous, "rotation must advance non-empty tiles");
    require(
        first.tile(rotatable).pathCount() == previousPaths.size(),
        "rotation must preserve the number of independent paths");
    for (std::size_t pathIndex = 0; pathIndex < previousPaths.size(); ++pathIndex) {
        require(
            first.tile(rotatable).paths()[pathIndex].firstSide ==
                    (previousPaths[pathIndex].firstSide + 1) % sideCount &&
                first.tile(rotatable).paths()[pathIndex].secondSide ==
                    (previousPaths[pathIndex].secondSide + 1) % sideCount,
            "rotation must advance both sides of every path together");
    }
    requireMetricsConsistent(first);
    requireMetricsConsistent(second);

    hexpuzzle::PuzzleBoard firstCandidate(planet, 23, {1});
    require(
        second.metrics().qualityScore >= firstCandidate.metrics().qualityScore,
        "candidate scoring must never choose a board worse than the first deterministic candidate");

    bool rejectedEmptyCandidateSet = false;
    try {
        hexpuzzle::PuzzleBoard invalid(planet, 23, {0});
        static_cast<void>(invalid);
    } catch (const std::invalid_argument&) {
        rejectedEmptyCandidateSet = true;
    }
    require(rejectedEmptyCandidateSet, "board generation must reject an empty candidate set");
}

void testPuzzleBoardConnectedPaths() {
    hexpuzzle::HexPlanet planet({2, 0.17f, 0.5f, 19});
    hexpuzzle::PuzzleBoard board(planet, 23);
    std::optional<std::size_t> connectedRoot;

    for (std::size_t id = 0; id < board.tileCount(); ++id) {
        const hexpuzzle::PuzzleTile& selected = board.tile(id);
        const std::vector<std::size_t> connectedSides = board.connectedSides(id);
        bool expectedActive = false;
        for (std::size_t side = 0; side < selected.sideCount(); ++side) {
            const hexpuzzle::PuzzleTile& adjacent = board.tile(selected.neighbor(side));
            const auto reciprocal = std::find(adjacent.neighbors().begin(), adjacent.neighbors().end(), id);
            require(reciprocal != adjacent.neighbors().end(), "tile adjacency must be reciprocal");
            const std::size_t adjacentSide = static_cast<std::size_t>(reciprocal - adjacent.neighbors().begin());
            const bool expected = selected.hasPort(side) && adjacent.hasPort(adjacentSide);
            const bool actual = std::find(connectedSides.begin(), connectedSides.end(), side) != connectedSides.end();
            require(actual == expected, "connections must match the reciprocal physical edge");
            expectedActive = expectedActive || expected;

            if (actual) {
                const std::vector<std::size_t> adjacentConnections = board.connectedSides(adjacent.id());
                require(
                    std::find(adjacentConnections.begin(), adjacentConnections.end(), adjacentSide) !=
                        adjacentConnections.end(),
                    "connected edges must be visible from both tiles");
            }
        }
        require(selected.active() == expectedActive, "active state must reflect reciprocal connections");
        if (!connectedSides.empty() && !connectedRoot.has_value()) {
            connectedRoot = id;
        }
    }

    require(connectedRoot.has_value(), "seeded board must contain a connected path");
    const std::vector<hexpuzzle::ConnectedRoute> routes = board.connectedRoutes(*connectedRoot);
    std::set<std::pair<std::size_t, std::size_t>> allSegments;
    std::set<std::size_t> rootPaths;
    bool reachedAnotherTile = false;
    require(!routes.empty(), "a selected tile must expose its independent routes");
    require(
        routes.size() <= board.tile(*connectedRoot).pathCount(),
        "externally joined paths may merge components but must not create extra routes");

    for (const hexpuzzle::ConnectedRoute& route : routes) {
        std::set<std::pair<std::size_t, std::size_t>> routeSegments;
        bool containsRootPath = false;
        require(!route.empty(), "connected routes must not be empty");
        for (const hexpuzzle::RouteSegment& segment : route) {
            require(segment.tileId < board.tileCount(), "route tile IDs must be valid");
            require(
                segment.pathIndex < board.tile(segment.tileId).pathCount(),
                "route path indices must be valid");
            const auto key = std::make_pair(segment.tileId, segment.pathIndex);
            require(routeSegments.insert(key).second, "a connected route must not repeat a tile path");
            require(allSegments.insert(key).second, "independent routes must not share a tile path");
            if (segment.tileId == *connectedRoot) {
                containsRootPath = true;
                rootPaths.insert(segment.pathIndex);
            } else {
                reachedAnotherTile = true;
            }
        }
        require(containsRootPath, "every returned route must contain a path from the selected tile");

        for (const hexpuzzle::RouteSegment& segment : route) {
            const hexpuzzle::PuzzleTile& selected = board.tile(segment.tileId);
            const hexpuzzle::ConnectorPath& path = selected.paths()[segment.pathIndex];
            const std::size_t sides[2]{path.firstSide, path.secondSide};
            const std::vector<std::size_t> connectedSides = board.connectedSides(segment.tileId);
            for (const std::size_t side : sides) {
                if (std::find(connectedSides.begin(), connectedSides.end(), side) == connectedSides.end()) {
                    continue;
                }
                const std::size_t adjacentId = selected.neighbor(side);
                const hexpuzzle::PuzzleTile& adjacent = board.tile(adjacentId);
                const auto reciprocal = std::find(adjacent.neighbors().begin(), adjacent.neighbors().end(), segment.tileId);
                require(reciprocal != adjacent.neighbors().end(), "route adjacency must remain reciprocal");
                const std::size_t adjacentSide =
                    static_cast<std::size_t>(reciprocal - adjacent.neighbors().begin());
                const std::optional<std::size_t> adjacentPath = adjacent.pathIndexForSide(adjacentSide);
                require(adjacentPath.has_value(), "connected sides must terminate at one adjacent path");
                require(
                    routeSegments.count(std::make_pair(adjacentId, *adjacentPath)) == 1,
                    "a route must contain every path reached through either endpoint");
            }
        }
    }
    require(
        rootPaths.size() == board.tile(*connectedRoot).pathCount(),
        "the selected tile's one, two, or three paths must all be represented");
    require(reachedAnotherTile, "a connected root route must reach another tile path");
}

void testOrbitCamera() {
    hexpuzzle::OrbitCamera camera(1024, 1024);
    require(
        (camera.pointerRayDirection() + camera.viewDirection()).length() < 0.0001f,
        "center pointer ray must point from the eye toward the planet");
    camera.setPointer({900, 200});
    const Imath::V3f before = camera.viewDirection();
    camera.update();
    require((camera.viewDirection() - before).length() > 0.0f, "off-center pointer must orbit camera");
    require(std::abs(camera.viewDirection().length() - 1.0f) < 0.0001f, "camera direction must stay normalized");
    require(std::abs(camera.viewDirection().dot(camera.upDirection())) < 0.0001f, "camera up must remain orthogonal");
}

void testBoundedAdamsBashforthOrbit() {
    hexpuzzle::BoundedAdamsBashforthOrbit orbit;
    Imath::V3d axis(0.6, 0.8, 0.2);
    axis.normalize();
    constexpr std::uint64_t framesPerRevolution = 3606;
    orbit.reset(
        {0.0, 0.0, 1.0},
        {0.0, 1.0, 0.0},
        axis,
        framesPerRevolution);

    double maximumNormError = 0.0;
    double maximumOrthogonalityError = 0.0;
    double maximumPredictionError = 0.0;
    double maximumReferenceError = 0.0;
    for (std::uint64_t frame = 0; frame < 1000000; ++frame) {
        orbit.update();
        const Imath::V3d& view = orbit.viewDirection();
        const Imath::V3d& up = orbit.upDirection();
        maximumNormError = std::max({
            maximumNormError,
            std::abs(view.length() - 1.0),
            std::abs(up.length() - 1.0),
        });
        maximumOrthogonalityError =
            std::max(maximumOrthogonalityError, std::abs(view.dot(up)));
        maximumPredictionError =
            std::max(maximumPredictionError, orbit.lastPredictionErrorRadians());
        maximumReferenceError =
            std::max(maximumReferenceError, orbit.lastReferenceErrorRadians());
        if (!orbit.reanchoredLastUpdate()) {
            require(
                orbit.lastReferenceErrorRadians() <=
                    (1.0 - orbit.correctionGain()) * orbit.lastPredictionErrorRadians() + 0.000000000001,
                "geodesic correction must contract the AB2 prediction error");
        }
        require(
            orbit.lastReferenceErrorRadians() <= orbit.errorBoundRadians(),
            "bounded AB2 reference error must not exceed its fail-closed cap");
    }

    require(maximumPredictionError > 0.0, "bounded AB2 must exercise a numerical predictor");
    require(
        maximumReferenceError < maximumPredictionError,
        "contractive correction must reduce the maximum AB2 prediction error");
    require(
        orbit.maximumReferenceErrorRadians() == maximumReferenceError,
        "bounded AB2 must report its maximum corrected reference error");
    require(maximumNormError < 0.000000000001, "quaternion projection must preserve unit vectors");
    require(
        maximumOrthogonalityError < 0.000000000001,
        "quaternion projection must preserve an orthogonal camera frame");
    require(
        orbit.periodicReanchorCount() == 1000000 / framesPerRevolution,
        "bounded AB2 must re-anchor at every complete revolution");
    require(orbit.safetyReanchorCount() == 0, "normal bounded AB2 operation must not need safety fallback");

    hexpuzzle::BoundedAdamsBashforthOrbit periodicOrbit;
    periodicOrbit.reset(
        {0.0, 0.0, 1.0},
        {0.0, 1.0, 0.0},
        axis,
        framesPerRevolution);
    for (std::uint64_t frame = 0; frame < framesPerRevolution; ++frame) {
        periodicOrbit.update();
    }
    require(
        (periodicOrbit.viewDirection() - Imath::V3d(0.0, 0.0, 1.0)).length() < 0.000000000000001,
        "periodic AB2 re-anchor must restore the exact anchor view");
    require(
        (periodicOrbit.upDirection() - Imath::V3d(0.0, 1.0, 0.0)).length() < 0.000000000000001,
        "periodic AB2 re-anchor must restore the exact anchor up direction");

    hexpuzzle::BoundedAdamsBashforthOrbit failClosedOrbit({0.25, 0.000000000000001});
    failClosedOrbit.reset(
        {0.0, 0.0, 1.0},
        {0.0, 1.0, 0.0},
        axis,
        framesPerRevolution);
    failClosedOrbit.update();
    require(failClosedOrbit.reanchoredLastUpdate(), "an exceeded AB2 error cap must re-anchor immediately");
    require(failClosedOrbit.safetyReanchorCount() == 1, "an exceeded AB2 error cap must be recorded");
    require(
        failClosedOrbit.lastReferenceErrorRadians() == 0.0,
        "fail-closed AB2 re-anchoring must restore the independent reference exactly");
}

void testOrbitCameraPoleContinuity() {
    hexpuzzle::OrbitCamera camera(1024, 1024);
    camera.setPointer({512, 0});
    float maximumVerticalDirection = camera.viewDirection().y;
    float minimumUpContinuity = 1.0f;
    for (int frame = 0; frame < 2000; ++frame) {
        const Imath::V3f previousUp = camera.upDirection();
        camera.update();
        maximumVerticalDirection = std::max(maximumVerticalDirection, camera.viewDirection().y);
        minimumUpContinuity = std::min(minimumUpContinuity, previousUp.dot(camera.upDirection()));
    }
    require(maximumVerticalDirection > 0.999f, "camera must pass through the pole instead of stalling below it");
    require(minimumUpContinuity > 0.999f, "camera up direction must not flip while crossing a pole");
}

void testOrbitCameraLongRunStability() {
    hexpuzzle::OrbitCamera camera(1024, 1024);
    const Imath::V3d initialView = asDouble(camera.viewDirection());
    const Imath::V3d initialUp = asDouble(camera.upDirection());
    camera.setPointer({900, 200});
    const std::uint64_t framesPerRevolution = camera.orbitFramesPerRevolution();
    require(framesPerRevolution > 0, "off-center orbit must define a frame-locked revolution");
    camera.update();

    const Imath::V3d oneStepView = asDouble(camera.viewDirection());
    const Imath::V3d oneStepUp = asDouble(camera.upDirection());
    const Imath::V3d oneStepRight = normalized(oneStepUp.cross(oneStepView));
    const Imath::V3d orbitAxis = normalized({
        oneStepUp.z - oneStepView.y,
        oneStepView.x - oneStepRight.z,
        oneStepRight.y - oneStepUp.x,
    });
    const double initialViewPlane = orbitAxis.dot(initialView);
    const double initialUpPlane = orbitAxis.dot(initialUp);
    double maximumPlaneError = 0.0;
    double maximumNormError = 0.0;
    double maximumOrthogonalityError = 0.0;

    for (int frame = 1; frame < 1000000; ++frame) {
        camera.update();
        const Imath::V3d view = asDouble(camera.viewDirection());
        const Imath::V3d up = asDouble(camera.upDirection());
        maximumPlaneError = std::max({
            maximumPlaneError,
            std::abs(orbitAxis.dot(normalized(view)) - initialViewPlane),
            std::abs(orbitAxis.dot(normalized(up)) - initialUpPlane),
        });
        maximumNormError = std::max({
            maximumNormError,
            std::abs(view.length() - 1.0),
            std::abs(up.length() - 1.0),
        });
        maximumOrthogonalityError =
            std::max(maximumOrthogonalityError, std::abs(view.dot(up)));
    }

    require(maximumPlaneError < 0.000001, "stationary orbit must remain on its original rotation planes");
    require(maximumNormError < 0.000001, "long stationary orbit must retain unit camera vectors");
    require(
        maximumOrthogonalityError < 0.000001,
        "long stationary orbit must retain an orthogonal camera basis");

    hexpuzzle::OrbitCamera periodicCamera(1024, 1024);
    const Imath::V3f initialPeriodicView = periodicCamera.viewDirection();
    const Imath::V3f initialPeriodicUp = periodicCamera.upDirection();
    periodicCamera.setPointer({900, 200});
    for (std::uint64_t frame = 0; frame < periodicCamera.orbitFramesPerRevolution(); ++frame) {
        periodicCamera.update();
    }
    require(
        (periodicCamera.viewDirection() - initialPeriodicView).length() == 0.0f,
        "one frame-locked revolution must return to the exact initial view");
    require(
        (periodicCamera.upDirection() - initialPeriodicUp).length() == 0.0f,
        "one frame-locked revolution must return to the exact initial up direction");
}

void testOrbitCameraViewportConsistency() {
    hexpuzzle::OrbitCamera resized(1024, 1024);
    resized.setPointer({512, 512});
    const Imath::V3f beforeResize = resized.viewDirection();
    resized.resize(2048, 2048);
    resized.update();
    require(
        (resized.viewDirection() - beforeResize).length() < 0.000001f,
        "resizing must preserve a centered pointer");

    hexpuzzle::OrbitCamera small(1024, 1024);
    hexpuzzle::OrbitCamera large(2048, 2048);
    small.setPointer({1024, 512});
    large.setPointer({2048, 1024});
    require(
        (small.pointerRayDirection() - large.pointerRayDirection()).length() < 0.000001f,
        "equal normalized pointer positions must produce equal picking rays");
    small.update();
    large.update();
    require(
        (small.viewDirection() - large.viewDirection()).length() < 0.000001f,
        "equal normalized pointer positions must produce equal orbit motion");
}

void testTileSequenceTracker() {
    hexpuzzle::TileSequenceTracker tracker({4, 2, 64});
    const std::array<std::size_t, 4> pattern{3, 8, 13, 21};
    for (const std::size_t tile : pattern) {
        tracker.observe(tile);
        tracker.observe(tile);
    }
    require(!tracker.repeating(), "one sequence traversal must not claim a repeat");
    for (std::size_t index = 0; index < pattern.size() - 1; ++index) {
        tracker.observe(pattern[index]);
    }
    require(!tracker.repeating(), "a matching prefix must not claim a repeated cycle");
    tracker.observe(pattern.back());
    require(tracker.repeating(), "two complete matching cycles must confirm repetition");
    require(tracker.repeatPeriod() == pattern.size(), "repeat period must match the original sequence");
    tracker.observe(pattern[0]);
    require(tracker.repeating(), "a confirmed sequence must remain active while matching");
    tracker.observe(99);
    require(!tracker.repeating(), "a sequence mismatch must clear the repeat state");
    tracker.reset();
    require(tracker.transitionCount() == 0, "reset must clear observed sequence transitions");
}

void testPlanetTileSequenceRepeats() {
    hexpuzzle::HexPlanet planet({4, 0.17f, 0.5f, 29});
    hexpuzzle::OrbitCamera camera(1024, 1024);
    hexpuzzle::TileSequenceTracker tracker;
    camera.setPointer({720, 300});
    bool wasRepeating = false;
    std::size_t repeatStarts = 0;
    std::size_t repeatStops = 0;
    const std::uint64_t frameCount = camera.orbitFramesPerRevolution() * 3;
    for (std::uint64_t frame = 0; frame < frameCount; ++frame) {
        camera.update();
        std::size_t selectedTile = 0;
        Imath::V3f hit;
        require(
            planet.tileIntersection(
                {camera.eyePosition(30.0f), camera.pointerRayDirection()},
                selectedTile,
                hit),
            "stationary sequence pointer must remain over the rendered planet");
        tracker.observe(selectedTile);
        if (!wasRepeating && tracker.repeating()) {
            ++repeatStarts;
        } else if (wasRepeating && !tracker.repeating()) {
            ++repeatStops;
        }
        wasRepeating = tracker.repeating();
    }
    require(tracker.repeating(), "a stationary pointer orbit must reproduce its tile sequence");
    require(tracker.repeatPeriod() >= 16, "a detected planet sequence must satisfy the minimum period");
    require(repeatStarts == 1, "a drift-free stationary orbit must establish one stable repeated sequence");
    require(repeatStops == 0, "a drift-free stationary orbit must not lose its repeated sequence");
}

void testDebugLog() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "hexpuzzle_debug_log_test.jsonl";
    std::filesystem::remove(path);
    {
        hexpuzzle::DebugLog log(std::optional<std::filesystem::path>{path});
        require(log.enabled(), "debug log must be enabled when a path is supplied");
        log.event("test_event", "quoted=\"value\"\nline=two");
    }
    {
        hexpuzzle::DebugLog log(std::optional<std::filesystem::path>{path});
        log.event("second_event");
    }

    std::ifstream input(path);
    std::string firstLine;
    std::string secondLine;
    require(static_cast<bool>(std::getline(input, firstLine)), "debug log must contain its first event");
    require(static_cast<bool>(std::getline(input, secondLine)), "debug log must append later events");
    require(
        firstLine.find("\"schema\":\"hexpuzzle.debug_event.v1\"") != std::string::npos,
        "debug log must identify its schema");
    require(firstLine.find("\"event\":\"test_event\"") != std::string::npos, "debug event name changed");
    require(
        firstLine.find("quoted=\\\"value\\\"\\nline=two") != std::string::npos,
        "debug messages must be JSON escaped");
    require(secondLine.find("\"event\":\"second_event\"") != std::string::npos, "debug log must append");
    std::filesystem::remove(path);
}

}  // namespace

int main() {
    try {
        testPlanetCountsAndTopology();
        testSelectionAndRayIntersection();
        testConnectorLayoutCatalogs();
        testSideConnectionCurves();
        testPuzzleBoardOwnershipAndDeterminism();
        testPuzzleBoardConnectedPaths();
        testOrbitCamera();
        testBoundedAdamsBashforthOrbit();
        testOrbitCameraPoleContinuity();
        testOrbitCameraLongRunStability();
        testOrbitCameraViewportConsistency();
        testTileSequenceTracker();
        testPlanetTileSequenceRepeats();
        testDebugLog();
        std::cout << "hexpuzzle_core_tests: PASS\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "hexpuzzle_core_tests: FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
