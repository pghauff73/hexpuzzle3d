#pragma once

#include "camera.h"
#include "hexplanet.h"
#include "puzzle.h"
#include "texture.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace hexpuzzle {

class HexPuzzleRenderer {
public:
    HexPuzzleRenderer(
        const HexPlanet& planet,
        const PuzzleBoard& board,
        const OrbitCamera& camera,
        const TextureLibrary& textures);

    void initializeOpenGL() const;
    void render(std::optional<std::size_t> selectedTile, bool sequenceRepeating) const;

private:
    void configureView() const;
    void drawTiles() const;
    void drawTile(const PuzzleTile& tile) const;
    void drawPathRibbon(
        const PuzzleTile& tile,
        const ConnectorPath& path,
        float surfaceOffset,
        const Imath::V3f& color,
        float alpha,
        float width) const;
    void drawConnectedRoutes(const std::vector<ConnectedRoute>& routes) const;
    void drawSelection(std::size_t selectedTile) const;
    void drawConnections(std::size_t selectedTile) const;
    void drawHud(
        std::optional<std::size_t> selectedTile,
        const std::vector<ConnectedRoute>& routes,
        std::size_t routeTileCount,
        bool sequenceRepeating) const;
    void drawScreenPanel(float left, float bottom, float right, float top, const Imath::V3f& color, float alpha) const;
    void drawText(const std::string& text, ScreenPoint position, const Imath::V3f& color, void* font) const;
    Imath::V3f sideAnchor(
        const PuzzleTile& tile,
        std::size_t side,
        float surfaceOffset,
        float centerInset) const;

    const HexPlanet& planet_;
    const PuzzleBoard& board_;
    const OrbitCamera& camera_;
    const TextureLibrary& textures_;
};

}  // namespace hexpuzzle
