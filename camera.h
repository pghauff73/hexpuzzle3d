#pragma once

#include "bounded_adams_bashforth.h"

#include <Imath/ImathVec.h>

#include <cstdint>

namespace hexpuzzle {

struct ScreenPoint {
    int x = 0;
    int y = 0;
};

class OrbitCamera {
public:
    static constexpr float verticalFieldOfViewDegrees = 50.0f;

    OrbitCamera(int viewportWidth = 1024, int viewportHeight = 1024);

    void resize(int width, int height) noexcept;
    void setPointer(ScreenPoint point) noexcept;
    void update();

    int viewportWidth() const noexcept;
    int viewportHeight() const noexcept;
    float aspectRatio() const noexcept;
    bool orbiting() const noexcept;
    std::uint64_t orbitFramesPerRevolution() const noexcept;
    double orbitCorrectionGain() const noexcept;
    double orbitErrorBoundRadians() const noexcept;
    double orbitPredictionErrorRadians() const noexcept;
    double orbitReferenceErrorRadians() const noexcept;
    double maximumOrbitReferenceErrorRadians() const noexcept;
    std::uint64_t orbitPeriodicReanchorCount() const noexcept;
    std::uint64_t orbitSafetyReanchorCount() const noexcept;
    const Imath::V3f& viewDirection() const noexcept;
    const Imath::V3f& upDirection() const noexcept;
    Imath::V3f eyePosition(float distance) const;
    Imath::V3f pointerRayDirection() const;

private:
    void resetOrbitAnchor() noexcept;
    void syncFromOrbitIntegrator() noexcept;
    void syncFloatDirections() noexcept;
    Imath::V3d rightDirection() const noexcept;
    double horizontalPointerOffset() const noexcept;
    double verticalPointerOffset() const noexcept;

    int viewportWidth_;
    int viewportHeight_;
    int pointerX_;
    int pointerY_;
    Imath::V3d viewDirectionDouble_{0.0, 0.0, 1.0};
    Imath::V3d upDirectionDouble_{0.0, 1.0, 0.0};
    BoundedAdamsBashforthOrbit orbitIntegrator_;
    Imath::V3f viewDirection_{0.0f, 0.0f, 1.0f};
    Imath::V3f upDirection_{0.0f, 1.0f, 0.0f};
};

}  // namespace hexpuzzle
