#pragma once

#include "hexplanet.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace hexpuzzle {

struct ConnectorPath {
    std::size_t firstSide = 0;
    std::size_t secondSide = 0;
};

using ConnectorLayout = std::vector<ConnectorPath>;

const std::vector<ConnectorLayout>& connectorLayoutCatalog(std::size_t sideCount);

struct RouteSegment {
    std::size_t tileId = 0;
    std::size_t pathIndex = 0;
};

using ConnectedRoute = std::vector<RouteSegment>;

struct PuzzleGenerationSettings {
    std::size_t candidateCount = 24;
};

struct BoardMetrics {
    std::size_t totalPaths = 0;
    std::size_t connectedPaths = 0;
    std::size_t connectedEdges = 0;
    std::size_t routeComponents = 0;
    std::size_t longestRoute = 0;
    std::size_t isolatedPaths = 0;
    std::int64_t qualityScore = 0;
};

class PuzzleTile {
public:
    PuzzleTile(
        std::size_t id,
        const Imath::V3f& center,
        std::vector<std::size_t> neighbors);

    std::size_t id() const noexcept;
    const Imath::V3f& center() const noexcept;
    const std::vector<std::size_t>& neighbors() const noexcept;
    std::size_t sideCount() const noexcept;
    std::size_t neighbor(std::size_t side) const;
    const std::vector<ConnectorPath>& paths() const noexcept;
    std::size_t pathCount() const noexcept;
    std::size_t rotation() const noexcept;
    bool active() const noexcept;

    void randomize(std::mt19937& random);
    void rotate();
    bool hasPort(std::size_t side) const;
    std::optional<std::size_t> pairedSide(std::size_t side) const noexcept;
    std::optional<std::size_t> pathIndexForSide(std::size_t side) const noexcept;
    void setActive(bool active) noexcept;

private:
    std::size_t id_;
    Imath::V3f center_;
    std::vector<std::size_t> neighbors_;
    std::vector<ConnectorPath> paths_;
    std::size_t rotation_ = 0;
    bool active_ = false;
};

class PuzzleBoard {
public:
    PuzzleBoard(
        const HexPlanet& planet,
        std::uint32_t randomSeed,
        PuzzleGenerationSettings generationSettings = {});

    std::size_t tileCount() const noexcept;
    const PuzzleTile& tile(std::size_t id) const;
    PuzzleTile& tile(std::size_t id);
    const std::vector<std::size_t>& traversalPath() const noexcept;

    void rotateTile(std::size_t id);
    void updateConnections();
    std::vector<std::size_t> connectedSides(std::size_t id) const;
    std::vector<ConnectedRoute> connectedRoutes(std::size_t startId) const;
    std::vector<ConnectedRoute> allRoutes() const;
    const BoardMetrics& metrics() const noexcept;

private:
    ConnectedRoute collectRoute(
        RouteSegment start,
        std::vector<std::vector<bool>>& visited) const;
    BoardMetrics calculateMetrics() const;
    void generateBoardCandidates();
    std::size_t reciprocalSide(std::size_t id, std::size_t side) const;
    bool isConnected(std::size_t id, std::size_t side) const;
    void buildTraversal(const HexPlanet& planet);
    void visit(std::size_t id, const HexPlanet& planet, std::vector<bool>& visited);
    void initializeTiles(const HexPlanet& planet);

    std::mt19937 random_;
    PuzzleGenerationSettings generationSettings_;
    std::vector<PuzzleTile> tiles_;
    std::vector<std::size_t> traversalPath_;
    BoardMetrics metrics_;
};

}  // namespace hexpuzzle
