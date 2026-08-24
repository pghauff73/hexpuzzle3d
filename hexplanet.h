#pragma once

#include <Imath/ImathVec.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

namespace hexpuzzle {

enum class TerrainType : std::uint8_t {
    Water,
    Desert,
    Grassland,
    Forest,
    Mountain,
};

struct HexTile {
    Imath::V3f position;
    Imath::V3f normal;
    TerrainType terrain = TerrainType::Desert;
    std::vector<std::size_t> incidentTriangles;
};

struct HexTriangle {
    static constexpr std::size_t invalidIndex = std::numeric_limits<std::size_t>::max();

    std::array<std::size_t, 3> vertices{};
    std::array<std::size_t, 3> neighbors{
        invalidIndex,
        invalidIndex,
        invalidIndex,
    };
    std::size_t subdivisionVertex = invalidIndex;
};

struct SurfaceOffset {
    float value = 0.0f;
};

struct Ray {
    Imath::V3f origin;
    Imath::V3f direction;
};

struct SideConnectionCurve {
    std::vector<Imath::V3f> points;
    bool straight = false;
};

class HexPlanet {
public:
    struct Settings {
        int subdivisionLevel = 4;
        float terrainRandomness = 0.17f;
        float waterProbability = 0.5f;
        std::uint32_t randomSeed = 0;
    };

    static constexpr float radius = 10.0f;

    HexPlanet();
    explicit HexPlanet(Settings settings);

    int subdivisionLevel() const noexcept;
    std::size_t tileCount() const noexcept;
    std::size_t triangleCount() const noexcept;

    const HexTile& tile(std::size_t index) const;
    const HexTriangle& triangle(std::size_t index) const;
    const std::vector<HexTile>& tiles() const noexcept;
    const std::vector<HexTriangle>& triangles() const noexcept;

    std::vector<Imath::V3f> polygon(
        std::size_t tileIndex,
        SurfaceOffset offset = SurfaceOffset{}) const;
    Imath::V3f sideAnchor(
        std::size_t tileIndex,
        std::size_t side,
        SurfaceOffset offset,
        float centerInset = 0.1f) const;
    SideConnectionCurve sideConnectionCurve(
        std::size_t tileIndex,
        std::size_t firstSide,
        std::size_t secondSide,
        SurfaceOffset offset,
        float centerInset = 0.1f,
        std::size_t segmentCount = 24) const;
    std::vector<std::size_t> neighbors(std::size_t tileIndex) const;
    std::size_t nearestTile(const Imath::V3f& surfaceDirection) const;
    bool rayIntersection(const Ray& ray, Imath::V3f& result) const;
    bool tileIntersection(
        const Ray& ray,
        std::size_t& tileIndex,
        Imath::V3f& result,
        SurfaceOffset offset = SurfaceOffset{1.0f}) const;

private:
    void buildLevelZero();
    void subdivide();
    void rebuildConnectivity();
    void projectToSphere();
    TerrainType randomTerrain();
    Imath::V3f triangleCenter(const HexTriangle& triangle) const;

    Settings settings_;
    int subdivisionLevel_ = 0;
    std::mt19937 random_;
    std::vector<HexTile> tiles_;
    std::vector<HexTriangle> triangles_;
};

}  // namespace hexpuzzle
