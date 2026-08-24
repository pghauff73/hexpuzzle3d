#include "renderer.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>

namespace hexpuzzle {
namespace {

Imath::V3f normalized(Imath::V3f value) {
    if (value.length() != 0.0f) {
        value.normalize();
    }
    return value;
}

void vertex(const Imath::V3f& point) {
    glVertex3f(point.x, point.y, point.z);
}

Imath::V3f routeColor(std::size_t routeIndex) {
    const std::array<Imath::V3f, 3> colors{
        Imath::V3f(0.15f, 0.9f, 1.0f),
        Imath::V3f(1.0f, 0.3f, 0.72f),
        Imath::V3f(0.38f, 1.0f, 0.48f),
    };
    return colors[routeIndex % colors.size()];
}

void drawRibbon(
    const std::vector<Imath::V3f>& points,
    const Imath::V3f& color,
    float alpha,
    float width) {
    if (points.size() < 2) {
        return;
    }
    const float halfWidth = width * 0.009f;
    Imath::V3f previousLateral;
    glColor4f(color.x, color.y, color.z, alpha);
    glBegin(GL_TRIANGLE_STRIP);
    for (std::size_t index = 0; index < points.size(); ++index) {
        const Imath::V3f normal = normalized(points[index]);
        const Imath::V3f tangent = normalized(
            index == 0 ? points[1] - points[0] :
            index + 1 == points.size() ? points[index] - points[index - 1] :
            points[index + 1] - points[index - 1]);
        Imath::V3f lateral = normalized(normal.cross(tangent));
        if (index != 0 && lateral.dot(previousLateral) < 0.0f) {
            lateral = -lateral;
        }
        previousLateral = lateral;
        const float radius = points[index].length();
        const Imath::V3f first = normalized(points[index] + lateral * halfWidth) * radius;
        const Imath::V3f second = normalized(points[index] - lateral * halfWidth) * radius;
        glNormal3f(normal.x, normal.y, normal.z);
        vertex(first);
        vertex(second);
    }
    glEnd();
}

void drawLine(
    const Imath::V3f& first,
    const Imath::V3f& second,
    const Imath::V3f& color,
    float alpha,
    float width) {
    constexpr int segmentCount = 12;
    const float firstRadius = first.length();
    const float secondRadius = second.length();
    std::vector<Imath::V3f> points;
    points.reserve(segmentCount + 1);
    for (int segment = 0; segment <= segmentCount; ++segment) {
        const float amount = static_cast<float>(segment) / static_cast<float>(segmentCount);
        const float radius = firstRadius * (1.0f - amount) + secondRadius * amount;
        points.push_back(normalized(first * (1.0f - amount) + second * amount) * radius);
    }
    drawRibbon(points, color, alpha, width);
}

void drawBridgeLine(
    const Imath::V3f& first,
    const Imath::V3f& edgeCenter,
    const Imath::V3f& second,
    const Imath::V3f& color,
    float alpha,
    float width) {
    constexpr int segmentCount = 6;
    std::vector<Imath::V3f> points;
    points.reserve(segmentCount * 2 + 1);
    for (int segment = 0; segment <= segmentCount; ++segment) {
        const float amount = static_cast<float>(segment) / static_cast<float>(segmentCount);
        points.push_back(
            normalized(first * (1.0f - amount) + edgeCenter * amount) *
            (first.length() * (1.0f - amount) + edgeCenter.length() * amount));
    }
    for (int segment = 1; segment <= segmentCount; ++segment) {
        const float amount = static_cast<float>(segment) / static_cast<float>(segmentCount);
        points.push_back(
            normalized(edgeCenter * (1.0f - amount) + second * amount) *
            (edgeCenter.length() * (1.0f - amount) + second.length() * amount));
    }
    drawRibbon(points, color, alpha, width);
}

void drawSurfaceDisc(
    const Imath::V3f& center,
    const Imath::V3f& normal,
    float radius,
    const Imath::V3f& color,
    float alpha) {
    const Imath::V3f reference =
        std::abs(normal.z) < 0.85f ? Imath::V3f(0.0f, 0.0f, 1.0f) : Imath::V3f(0.0f, 1.0f, 0.0f);
    const Imath::V3f tangent = normalized(normal.cross(reference));
    const Imath::V3f bitangent = normalized(normal.cross(tangent));
    const float surfaceRadius = center.length();
    constexpr int segmentCount = 16;

    glColor4f(color.x, color.y, color.z, alpha);
    glBegin(GL_TRIANGLE_FAN);
    vertex(center);
    for (int segment = 0; segment <= segmentCount; ++segment) {
        const float angle =
            static_cast<float>(segment) * 2.0f * 3.14159265358979323846f /
            static_cast<float>(segmentCount);
        const Imath::V3f point =
            normalized(
                center + tangent * (std::cos(angle) * radius) +
                bitangent * (std::sin(angle) * radius)) *
            surfaceRadius;
        vertex(point);
    }
    glEnd();
}

}  // namespace

HexPuzzleRenderer::HexPuzzleRenderer(
    const HexPlanet& planet,
    const PuzzleBoard& board,
    const OrbitCamera& camera,
    const TextureLibrary& textures)
    : planet_(planet), board_(board), camera_(camera), textures_(textures) {
}

void HexPuzzleRenderer::initializeOpenGL() const {
    glClearColor(0.006f, 0.012f, 0.024f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.1f);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glDisable(GL_CULL_FACE);
    glShadeModel(GL_SMOOTH);

    const GLfloat ambient[] = {0.12f, 0.16f, 0.22f, 1.0f};
    const GLfloat diffuse[] = {0.82f, 0.9f, 1.0f, 1.0f};
    const GLfloat specular[] = {0.35f, 0.5f, 0.7f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
}

void HexPuzzleRenderer::render(
    std::optional<std::size_t> selectedTile,
    bool sequenceRepeating) const {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    configureView();
    drawTiles();

    std::vector<ConnectedRoute> connectedRoutes;
    std::size_t routeTileCount = 0;
    if (selectedTile.has_value()) {
        connectedRoutes = board_.connectedRoutes(*selectedTile);
        std::vector<bool> routeTiles(board_.tileCount(), false);
        for (const ConnectedRoute& route : connectedRoutes) {
            for (const RouteSegment& segment : route) {
                if (!routeTiles[segment.tileId]) {
                    routeTiles[segment.tileId] = true;
                    ++routeTileCount;
                }
            }
        }
        drawConnectedRoutes(connectedRoutes);
        drawSelection(*selectedTile);
        drawConnections(*selectedTile);
    }
    drawHud(selectedTile, connectedRoutes, routeTileCount, sequenceRepeating);
    glutSwapBuffers();
}

void HexPuzzleRenderer::configureView() const {
    glViewport(0, 0, camera_.viewportWidth(), camera_.viewportHeight());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(OrbitCamera::verticalFieldOfViewDegrees, camera_.aspectRatio(), 1.0, 200.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const Imath::V3f eye = camera_.eyePosition(30.0f);
    const Imath::V3f up = camera_.upDirection();
    gluLookAt(eye.x, eye.y, eye.z, 0.0, 0.0, 0.0, up.x, up.y, up.z);

    const Imath::V3f light = camera_.viewDirection() * 40.0f;
    const GLfloat lightPosition[] = {light.x, light.y, light.z, 0.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
}

void HexPuzzleRenderer::drawTiles() const {
    for (std::size_t id = 0; id < board_.tileCount(); ++id) {
        drawTile(board_.tile(id));
    }
}

void HexPuzzleRenderer::drawTile(const PuzzleTile& tile) const {
    const std::vector<Imath::V3f> shellPolygon =
        planet_.polygon(tile.id(), SurfaceOffset{1.0f});
    const std::vector<Imath::V3f> facePolygon =
        planet_.polygon(tile.id(), SurfaceOffset{1.025f});
    const Imath::V3f faceCenter = tile.center() * (HexPlanet::radius + 1.025f);

    glEnable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.025f, 0.055f, 0.085f, 1.0f);
    glBegin(GL_POLYGON);
    glNormal3f(tile.center().x, tile.center().y, tile.center().z);
    for (const Imath::V3f& point : shellPolygon) {
        vertex(point);
    }
    glEnd();

    if (tile.active()) {
        glColor4f(0.16f, 0.25f, 0.31f, 1.0f);
    } else {
        glColor4f(0.105f, 0.14f, 0.19f, 1.0f);
    }
    glBegin(GL_POLYGON);
    glNormal3f(tile.center().x, tile.center().y, tile.center().z);
    for (const Imath::V3f& point : facePolygon) {
        vertex(
            normalized(point * 0.925f + faceCenter * 0.075f) *
            (HexPlanet::radius + 1.025f));
    }
    glEnd();

    glDisable(GL_LIGHTING);
    glColor4f(
        tile.active() ? 0.12f : 0.06f,
        tile.active() ? 0.58f : 0.14f,
        tile.active() ? 0.72f : 0.22f,
        tile.active() ? 0.9f : 0.8f);
    glLineWidth(tile.active() ? 2.4f : 1.4f);
    glBegin(GL_LINE_LOOP);
    for (const Imath::V3f& point : planet_.polygon(tile.id(), SurfaceOffset{1.065f})) {
        vertex(point);
    }
    glEnd();

    const Imath::V3f pathCasing(0.012f, 0.025f, 0.04f);
    const Imath::V3f pathCore =
        tile.active() ? Imath::V3f(0.35f, 0.58f, 0.68f) : Imath::V3f(0.23f, 0.34f, 0.43f);
    glDepthFunc(GL_LEQUAL);
    for (const ConnectorPath& path : tile.paths()) {
        drawPathRibbon(tile, path, 1.13f, pathCasing, 1.0f, 10.0f);
        drawPathRibbon(tile, path, 1.13f, pathCore, 1.0f, 3.8f);

        const Imath::V3f firstPort = sideAnchor(tile, path.firstSide, 1.145f, 0.1f);
        const Imath::V3f secondPort = sideAnchor(tile, path.secondSide, 1.145f, 0.1f);
        drawSurfaceDisc(firstPort, tile.center(), 0.13f, pathCasing, 1.0f);
        drawSurfaceDisc(secondPort, tile.center(), 0.13f, pathCasing, 1.0f);
        drawSurfaceDisc(firstPort, tile.center(), 0.065f, pathCore, 1.0f);
        drawSurfaceDisc(secondPort, tile.center(), 0.065f, pathCore, 1.0f);
    }
    glDepthFunc(GL_LESS);
}

void HexPuzzleRenderer::drawPathRibbon(
    const PuzzleTile& tile,
    const ConnectorPath& path,
    float surfaceOffset,
    const Imath::V3f& color,
    float alpha,
    float width) const {
    const SideConnectionCurve curve = planet_.sideConnectionCurve(
        tile.id(),
        path.firstSide,
        path.secondSide,
        SurfaceOffset{surfaceOffset});
    drawRibbon(curve.points, color, alpha, width);
}

void HexPuzzleRenderer::drawConnectedRoutes(
    const std::vector<ConnectedRoute>& routes) const {
    if (routes.empty()) {
        return;
    }

    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glDepthFunc(GL_LEQUAL);

    for (std::size_t routeIndex = 0; routeIndex < routes.size(); ++routeIndex) {
        const Imath::V3f color = routeColor(routeIndex);
        for (const RouteSegment& segment : routes[routeIndex]) {
            const PuzzleTile& tile = board_.tile(segment.tileId);
            const ConnectorPath& path = tile.paths().at(segment.pathIndex);
            drawPathRibbon(tile, path, 1.19f, color, 0.22f, 13.0f);

            const Imath::V3f first = sideAnchor(tile, path.firstSide, 1.235f, 0.1f);
            const Imath::V3f second = sideAnchor(tile, path.secondSide, 1.235f, 0.1f);
            drawPathRibbon(tile, path, 1.235f, Imath::V3f(0.01f, 0.02f, 0.03f), 0.95f, 8.5f);
            drawPathRibbon(tile, path, 1.235f, color, 1.0f, 4.5f);
            drawSurfaceDisc(first, tile.center(), 0.105f, Imath::V3f(0.01f, 0.02f, 0.03f), 1.0f);
            drawSurfaceDisc(second, tile.center(), 0.105f, Imath::V3f(0.01f, 0.02f, 0.03f), 1.0f);
            drawSurfaceDisc(first, tile.center(), 0.06f, color, 1.0f);
            drawSurfaceDisc(second, tile.center(), 0.06f, color, 1.0f);
        }

        for (const RouteSegment& segment : routes[routeIndex]) {
            const PuzzleTile& current = board_.tile(segment.tileId);
            const ConnectorPath& path = current.paths().at(segment.pathIndex);
            const std::size_t pathSides[2]{path.firstSide, path.secondSide};
            const std::vector<std::size_t> connected = board_.connectedSides(segment.tileId);
            for (const std::size_t side : pathSides) {
                if (std::find(connected.begin(), connected.end(), side) == connected.end()) {
                    continue;
                }
                const std::size_t adjacentId = current.neighbor(side);
                if (segment.tileId >= adjacentId) {
                    continue;
                }
                const PuzzleTile& adjacent = board_.tile(adjacentId);
                const auto reciprocal =
                    std::find(adjacent.neighbors().begin(), adjacent.neighbors().end(), segment.tileId);
                if (reciprocal == adjacent.neighbors().end()) {
                    continue;
                }
                const std::size_t adjacentSide =
                    static_cast<std::size_t>(reciprocal - adjacent.neighbors().begin());
                const Imath::V3f currentAnchor = sideAnchor(current, side, 1.3f, 0.1f);
                const Imath::V3f adjacentAnchor = sideAnchor(adjacent, adjacentSide, 1.3f, 0.1f);
                const Imath::V3f edgeCenter =
                    normalized((currentAnchor + adjacentAnchor) * 0.5f) *
                    (HexPlanet::radius + 1.3f);
                drawBridgeLine(
                    currentAnchor,
                    edgeCenter,
                    adjacentAnchor,
                    Imath::V3f(0.035f, 0.03f, 0.01f),
                    0.95f,
                    11.0f);
                drawBridgeLine(
                    currentAnchor,
                    edgeCenter,
                    adjacentAnchor,
                    Imath::V3f(1.0f, 0.82f, 0.12f),
                    1.0f,
                    4.5f);
                drawSurfaceDisc(
                    currentAnchor,
                    current.center(),
                    0.075f,
                    Imath::V3f(1.0f, 0.82f, 0.12f),
                    1.0f);
                drawSurfaceDisc(
                    adjacentAnchor,
                    adjacent.center(),
                    0.075f,
                    Imath::V3f(1.0f, 0.82f, 0.12f),
                    1.0f);
            }
        }
    }

    glDepthFunc(GL_LESS);
}

void HexPuzzleRenderer::drawSelection(std::size_t selectedTile) const {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor4f(1.0f, 0.67f, 0.12f, 0.22f);
    glLineWidth(13.0f);
    glBegin(GL_LINE_LOOP);
    for (const Imath::V3f& point : planet_.polygon(selectedTile, SurfaceOffset{1.31f})) {
        vertex(point);
    }
    glEnd();

    glColor4f(1.0f, 0.82f, 0.28f, 1.0f);
    glLineWidth(3.5f);
    glBegin(GL_LINE_LOOP);
    for (const Imath::V3f& point : planet_.polygon(selectedTile, SurfaceOffset{1.345f})) {
        vertex(point);
    }
    glEnd();
}

void HexPuzzleRenderer::drawConnections(std::size_t selectedTile) const {
    const PuzzleTile& selected = board_.tile(selectedTile);
    const std::vector<std::size_t> connectedSides = board_.connectedSides(selectedTile);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    for (const ConnectorPath& path : selected.paths()) {
        const std::size_t sides[2]{path.firstSide, path.secondSide};
        for (const std::size_t side : sides) {
            const bool connected =
                std::find(connectedSides.begin(), connectedSides.end(), side) != connectedSides.end();
            const Imath::V3f color = connected ?
                Imath::V3f(1.0f, 0.82f, 0.12f) :
                Imath::V3f(0.48f, 0.62f, 0.72f);
            const Imath::V3f port = sideAnchor(selected, side, 1.37f, 0.1f);
            const Imath::V3f edge = sideAnchor(selected, side, 1.37f, 0.015f);
            drawLine(port, edge, Imath::V3f(0.01f, 0.02f, 0.03f), 1.0f, 7.0f);
            drawLine(port, edge, color, 1.0f, connected ? 3.5f : 2.0f);
            drawSurfaceDisc(port, selected.center(), 0.11f, Imath::V3f(0.01f, 0.02f, 0.03f), 1.0f);
            drawSurfaceDisc(port, selected.center(), connected ? 0.065f : 0.045f, color, 1.0f);
        }
    }
}

void HexPuzzleRenderer::drawHud(
    std::optional<std::size_t> selectedTile,
    const std::vector<ConnectedRoute>& routes,
    std::size_t routeTileCount,
    bool sequenceRepeating) const {
    const int panelWidth = std::max(300, std::min(760, camera_.viewportWidth() - 24));
    const int panelHeight = selectedTile.has_value() ? 104 : 82;
    drawScreenPanel(
        12.0f,
        12.0f,
        static_cast<float>(12 + panelWidth),
        static_cast<float>(12 + panelHeight),
        Imath::V3f(0.015f, 0.035f, 0.06f),
        0.9f);

    drawText(
        "HEX PATH NETWORK",
        {26, 12 + panelHeight - 24},
        {1.0f, 0.78f, 0.22f},
        GLUT_BITMAP_HELVETICA_18);

    std::ostringstream selectionStatus;
    if (selectedTile.has_value()) {
        selectionStatus
            << "TILE " << *selectedTile << '/' << planet_.tileCount() - 1
            << "  |  " << board_.tile(*selectedTile).pathCount() << " PATHS"
            << "  |  " << routes.size() << " ROUTES"
            << "  |  " << routeTileCount << " ROUTE TILES";
    } else {
        selectionStatus << "NO TILE UNDER POINTER";
    }
    drawText(
        selectionStatus.str(),
        {26, 12 + panelHeight - 47},
        selectedTile.has_value() ? Imath::V3f(0.72f, 0.9f, 1.0f) : Imath::V3f(0.62f, 0.7f, 0.78f),
        GLUT_BITMAP_8_BY_13);

    const BoardMetrics& metrics = board_.metrics();
    std::ostringstream boardStatus;
    boardStatus
        << "BOARD  " << metrics.connectedPaths << '/' << metrics.totalPaths << " PATHS LINKED"
        << "  |  " << metrics.connectedEdges << " EDGES"
        << "  |  LONGEST " << metrics.longestRoute
        << "  |  " << metrics.routeComponents << " COMPONENTS"
        << "  |  SCORE " << metrics.qualityScore;
    drawText(
        boardStatus.str(),
        {26, 12 + panelHeight - 68},
        {0.45f, 0.72f, 0.78f},
        GLUT_BITMAP_8_BY_13);

    if (selectedTile.has_value()) {
        drawText(
            "MOVE POINTER: ORBIT + PICK     LEFT CLICK: ROTATE",
            {26, 26},
            {0.58f, 0.62f, 0.68f},
            GLUT_BITMAP_8_BY_13);
    }

    if (sequenceRepeating) {
        drawScreenPanel(
            12.0f,
            static_cast<float>(camera_.viewportHeight() - 54),
            136.0f,
            static_cast<float>(camera_.viewportHeight() - 12),
            Imath::V3f(0.04f, 0.22f, 0.15f),
            0.9f);
        drawText(
            "REPEAT",
            {28, camera_.viewportHeight() - 40},
            {0.22f, 1.0f, 0.55f},
            GLUT_BITMAP_HELVETICA_18);
    }
}

void HexPuzzleRenderer::drawScreenPanel(
    float left,
    float bottom,
    float right,
    float top,
    const Imath::V3f& color,
    float alpha) const {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, camera_.viewportWidth(), 0, camera_.viewportHeight(), -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glColor4f(color.x, color.y, color.z, alpha);
    glBegin(GL_QUADS);
    glVertex2f(left, bottom);
    glVertex2f(right, bottom);
    glVertex2f(right, top);
    glVertex2f(left, top);
    glEnd();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

Imath::V3f HexPuzzleRenderer::sideAnchor(
    const PuzzleTile& tile,
    std::size_t side,
    float surfaceOffset,
    float centerInset) const {
    return planet_.sideAnchor(
        tile.id(),
        side,
        SurfaceOffset{surfaceOffset},
        centerInset);
}

void HexPuzzleRenderer::drawText(
    const std::string& text,
    ScreenPoint position,
    const Imath::V3f& color,
    void* font) const {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, camera_.viewportWidth(), 0, camera_.viewportHeight(), -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glColor3f(color.x, color.y, color.z);
    glRasterPos2i(position.x, position.y);
    for (const unsigned char character : text) {
        glutBitmapCharacter(font, character);
    }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

}  // namespace hexpuzzle
