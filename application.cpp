#include "application.h"

#include <GL/freeglut.h>

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace hexpuzzle {

HexPuzzleApplication* HexPuzzleApplication::instance_ = nullptr;

HexPuzzleApplication::HexPuzzleApplication(Options options)
    : options_(std::move(options)),
      debugLog_(options_.debugLogPath),
      planet_({
          options_.subdivisionLevel,
          0.17f,
          0.5f,
          options_.randomSeed ^ 0x9e3779b9U,
      }),
      board_(planet_, options_.randomSeed),
      camera_(options_.width, options_.height),
      renderer_(planet_, board_, camera_, textures_),
      pointer_{options_.width / 2, options_.height / 2} {
}

int HexPuzzleApplication::run(int argc, char** argv) {
    if (instance_ != nullptr) {
        throw std::runtime_error("only one HexPuzzleApplication may run at a time");
    }
    instance_ = this;

    std::ostringstream startup;
    startup << "seed=" << options_.randomSeed << " subdivisions=" << options_.subdivisionLevel
            << " viewport=" << options_.width << 'x' << options_.height
            << " assets=" << options_.assetDirectory.string()
            << " smoke_test=" << (options_.smokeTest ? "true" : "false")
            << " board_paths=" << board_.metrics().totalPaths
            << " connected_paths=" << board_.metrics().connectedPaths
            << " connected_edges=" << board_.metrics().connectedEdges
            << " route_components=" << board_.metrics().routeComponents
            << " longest_route=" << board_.metrics().longestRoute
            << " board_score=" << board_.metrics().qualityScore;
    debugLog_.event("application_starting", startup.str());

    try {
        glutInit(&argc, argv);
        glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
        glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);
        glutInitWindowPosition(0, 0);
        glutInitWindowSize(options_.width, options_.height);
        glutCreateWindow("HexPuzzlePlanet");

        renderer_.initializeOpenGL();
        textures_.load(options_.assetDirectory);
        debugLog_.event("graphics_initialized", "textures_loaded=true");

        glutDisplayFunc(displayCallback);
        glutReshapeFunc(reshapeCallback);
        glutPassiveMotionFunc(pointerCallback);
        glutMotionFunc(pointerCallback);
        glutMouseFunc(mouseCallback);
        glutIdleFunc(idleCallback);
        debugLog_.event("application_running");
        glutMainLoop();
        debugLog_.event("application_stopped", "reason=main_loop_returned");
        instance_ = nullptr;
        return 0;
    } catch (const std::exception& error) {
        debugLog_.event("application_error", error.what());
        instance_ = nullptr;
        throw;
    } catch (...) {
        debugLog_.event("application_error", "unknown exception");
        instance_ = nullptr;
        throw;
    }
}

void HexPuzzleApplication::displayCallback() {
    instance_->display();
}

void HexPuzzleApplication::reshapeCallback(int width, int height) {
    instance_->reshape(width, height);
}

void HexPuzzleApplication::pointerCallback(int x, int y) {
    instance_->setPointer({x, y});
}

void HexPuzzleApplication::mouseCallback(int button, int state, int x, int y) {
    instance_->mouse({button, state, {x, y}});
}

void HexPuzzleApplication::idleCallback() {
    glutPostRedisplay();
}

void HexPuzzleApplication::display() {
    const Imath::V3f previousUp = camera_.upDirection();
    camera_.update();
    ++frameCount_;
    const float upContinuity = previousUp.dot(camera_.upDirection());
    if (frameCount_ == 1 || frameCount_ % 120 == 0) {
        logCameraState("camera_state", upContinuity);
    }
    if (upContinuity < 0.0f && frameCount_ - lastCameraWarningFrame_ >= 60) {
        logCameraState("camera_basis_discontinuity", upContinuity);
        lastCameraWarningFrame_ = frameCount_;
    }
    std::size_t pickedTile = 0;
    Imath::V3f pickedPoint;
    const Ray pointerRay{
        camera_.eyePosition(30.0f),
        camera_.pointerRayDirection(),
    };
    if (planet_.tileIntersection(pointerRay, pickedTile, pickedPoint)) {
        selectedTile_ = pickedTile;
    } else {
        selectedTile_.reset();
    }
    updateSequenceRepeatState();
    if (rotateRequested_) {
        if (selectedTile_.has_value()) {
            board_.rotateTile(*selectedTile_);
            debugLog_.event(
                "tile_rotated",
                "tile=" + std::to_string(*selectedTile_) +
                    " connected_edges=" + std::to_string(board_.metrics().connectedEdges) +
                    " longest_route=" + std::to_string(board_.metrics().longestRoute) +
                    " board_score=" + std::to_string(board_.metrics().qualityScore));
        } else {
            debugLog_.event("tile_rotation_ignored", "reason=pointer_missed_planet");
        }
        rotateRequested_ = false;
    }
    renderer_.render(selectedTile_, sequenceRepeating_);
    if (options_.smokeTest && !smokeFrameRendered_) {
        smokeFrameRendered_ = true;
        debugLog_.event("smoke_frame_rendered", "frame=" + std::to_string(frameCount_));
        glutLeaveMainLoop();
    }
}

void HexPuzzleApplication::reshape(int width, int height) {
    camera_.resize(width, height);
    debugLog_.event(
        "viewport_resized",
        "width=" + std::to_string(camera_.viewportWidth()) +
            " height=" + std::to_string(camera_.viewportHeight()));
}

void HexPuzzleApplication::setPointer(ScreenPoint point) {
    if (point.x != pointer_.x || point.y != pointer_.y) {
        stopSequenceTracking("pointer_moved");
        pointer_ = point;
    }
    camera_.setPointer(point);
}

void HexPuzzleApplication::mouse(MouseEvent event) {
    setPointer(event.point);
    if (event.button == GLUT_LEFT_BUTTON && event.state == GLUT_DOWN) {
        rotateRequested_ = true;
    }
}

void HexPuzzleApplication::logCameraState(std::string_view event, float upContinuity) {
    const Imath::V3f& view = camera_.viewDirection();
    const Imath::V3f& up = camera_.upDirection();
    const Imath::V3f pointer = camera_.pointerRayDirection();
    std::ostringstream message;
    message << "frame=" << frameCount_ << " view=" << view.x << ',' << view.y << ',' << view.z
            << " up=" << up.x << ',' << up.y << ',' << up.z
            << " pointer_ray=" << pointer.x << ',' << pointer.y << ',' << pointer.z
            << " orbit_integrator=adams_bashforth_bounded_v1"
            << " orbit_frames_per_revolution=" << camera_.orbitFramesPerRevolution()
            << " orbit_correction_gain=" << camera_.orbitCorrectionGain()
            << " orbit_prediction_error_radians=" << camera_.orbitPredictionErrorRadians()
            << " orbit_reference_error_radians=" << camera_.orbitReferenceErrorRadians()
            << " orbit_error_bound_radians=" << camera_.orbitErrorBoundRadians()
            << " orbit_max_reference_error_radians=" << camera_.maximumOrbitReferenceErrorRadians()
            << " orbit_periodic_reanchors=" << camera_.orbitPeriodicReanchorCount()
            << " orbit_safety_reanchors=" << camera_.orbitSafetyReanchorCount()
            << " view_length_error=" << std::abs(view.length() - 1.0f)
            << " up_length_error=" << std::abs(up.length() - 1.0f)
            << " orthogonality_error=" << std::abs(view.dot(up))
            << " up_continuity=" << upContinuity;
    debugLog_.event(event, message.str());
}

void HexPuzzleApplication::updateSequenceRepeatState() {
    if (!camera_.orbiting()) {
        stopSequenceTracking("orbit_stopped");
        return;
    }
    if (!selectedTile_.has_value()) {
        stopSequenceTracking("pointer_missed_planet");
        return;
    }

    const bool wasRepeating = sequenceRepeating_;
    sequenceTracker_.observe(*selectedTile_);
    sequenceRepeating_ = sequenceTracker_.repeating();
    if (!wasRepeating && sequenceRepeating_) {
        debugLog_.event(
            "tile_sequence_repeat_started",
            "period=" + std::to_string(sequenceTracker_.repeatPeriod()) +
                " transitions=" + std::to_string(sequenceTracker_.transitionCount()));
    } else if (wasRepeating && !sequenceRepeating_) {
        debugLog_.event("tile_sequence_repeat_stopped", "reason=sequence_mismatch");
    }
}

void HexPuzzleApplication::stopSequenceTracking(std::string_view reason) {
    if (sequenceRepeating_) {
        debugLog_.event("tile_sequence_repeat_stopped", "reason=" + std::string(reason));
    }
    sequenceTracker_.reset();
    sequenceRepeating_ = false;
}

}  // namespace hexpuzzle
