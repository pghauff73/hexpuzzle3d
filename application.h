#pragma once

#include "camera.h"
#include "debug_log.h"
#include "hexplanet.h"
#include "puzzle.h"
#include "renderer.h"
#include "sequence_tracker.h"
#include "texture.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

namespace hexpuzzle {

class HexPuzzleApplication {
public:
    struct Options {
        int width = 1024;
        int height = 1024;
        int subdivisionLevel = 4;
        std::uint32_t randomSeed = 0;
        std::filesystem::path assetDirectory = ".";
        std::optional<std::filesystem::path> debugLogPath;
        bool smokeTest = false;
    };

    explicit HexPuzzleApplication(Options options);
    int run(int argc, char** argv);

private:
    struct MouseEvent {
        int button = 0;
        int state = 0;
        ScreenPoint point;
    };

    static void displayCallback();
    static void reshapeCallback(int width, int height);
    static void pointerCallback(int x, int y);
    static void mouseCallback(int button, int state, int x, int y);
    static void idleCallback();

    void display();
    void reshape(int width, int height);
    void setPointer(ScreenPoint point);
    void mouse(MouseEvent event);
    void logCameraState(std::string_view event, float upContinuity);
    void updateSequenceRepeatState();
    void stopSequenceTracking(std::string_view reason);

    static HexPuzzleApplication* instance_;

    Options options_;
    DebugLog debugLog_;
    HexPlanet planet_;
    PuzzleBoard board_;
    OrbitCamera camera_;
    TextureLibrary textures_;
    HexPuzzleRenderer renderer_;
    TileSequenceTracker sequenceTracker_;
    ScreenPoint pointer_;
    std::optional<std::size_t> selectedTile_;
    bool sequenceRepeating_ = false;
    bool rotateRequested_ = false;
    bool smokeFrameRendered_ = false;
    std::uint64_t frameCount_ = 0;
    std::uint64_t lastCameraWarningFrame_ = 0;
};

}  // namespace hexpuzzle
