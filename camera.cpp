#include "camera.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace hexpuzzle {
namespace {

constexpr long double twoPi = 6.283185307179586476925286766559005768L;

Imath::V3d normalized(Imath::V3d value) noexcept {
    if (value.length() != 0.0) {
        value.normalize();
    }
    return value;
}

Imath::V3d rotateAroundAxis(
    const Imath::V3d& value,
    const Imath::V3d& normalizedAxis,
    double angle) {
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    return value * cosine + normalizedAxis.cross(value) * sine +
        normalizedAxis * (normalizedAxis.dot(value) * (1.0 - cosine));
}

int scaledCoordinate(int coordinate, int oldExtent, int newExtent) noexcept {
    const double scaled = static_cast<double>(coordinate) * static_cast<double>(newExtent) /
        static_cast<double>(oldExtent);
    const double bounded = std::clamp(
        scaled,
        static_cast<double>(std::numeric_limits<int>::min()),
        static_cast<double>(std::numeric_limits<int>::max()));
    return static_cast<int>(std::lround(bounded));
}

}  // namespace

OrbitCamera::OrbitCamera(int viewportWidth, int viewportHeight)
    : viewportWidth_(std::max(1, viewportWidth)),
      viewportHeight_(std::max(1, viewportHeight)),
      pointerX_(viewportWidth_ / 2),
      pointerY_(viewportHeight_ / 2) {
    resetOrbitAnchor();
}

void OrbitCamera::resize(int width, int height) noexcept {
    const int newWidth = std::max(1, width);
    const int newHeight = std::max(1, height);
    pointerX_ = scaledCoordinate(pointerX_, viewportWidth_, newWidth);
    pointerY_ = scaledCoordinate(pointerY_, viewportHeight_, newHeight);
    viewportWidth_ = newWidth;
    viewportHeight_ = newHeight;
    resetOrbitAnchor();
}

void OrbitCamera::setPointer(ScreenPoint point) noexcept {
    if (pointerX_ == point.x && pointerY_ == point.y) {
        return;
    }
    pointerX_ = point.x;
    pointerY_ = point.y;
    resetOrbitAnchor();
}

void OrbitCamera::update() {
    orbitIntegrator_.update();
    syncFromOrbitIntegrator();
}

int OrbitCamera::viewportWidth() const noexcept {
    return viewportWidth_;
}

int OrbitCamera::viewportHeight() const noexcept {
    return viewportHeight_;
}

float OrbitCamera::aspectRatio() const noexcept {
    return static_cast<float>(viewportWidth_) / static_cast<float>(viewportHeight_);
}

bool OrbitCamera::orbiting() const noexcept {
    return orbitIntegrator_.active();
}

std::uint64_t OrbitCamera::orbitFramesPerRevolution() const noexcept {
    return orbitIntegrator_.framesPerRevolution();
}

double OrbitCamera::orbitCorrectionGain() const noexcept {
    return orbitIntegrator_.correctionGain();
}

double OrbitCamera::orbitErrorBoundRadians() const noexcept {
    return orbitIntegrator_.errorBoundRadians();
}

double OrbitCamera::orbitPredictionErrorRadians() const noexcept {
    return orbitIntegrator_.lastPredictionErrorRadians();
}

double OrbitCamera::orbitReferenceErrorRadians() const noexcept {
    return orbitIntegrator_.lastReferenceErrorRadians();
}

double OrbitCamera::maximumOrbitReferenceErrorRadians() const noexcept {
    return orbitIntegrator_.maximumReferenceErrorRadians();
}

std::uint64_t OrbitCamera::orbitPeriodicReanchorCount() const noexcept {
    return orbitIntegrator_.periodicReanchorCount();
}

std::uint64_t OrbitCamera::orbitSafetyReanchorCount() const noexcept {
    return orbitIntegrator_.safetyReanchorCount();
}

const Imath::V3f& OrbitCamera::viewDirection() const noexcept {
    return viewDirection_;
}

const Imath::V3f& OrbitCamera::upDirection() const noexcept {
    return upDirection_;
}

Imath::V3f OrbitCamera::eyePosition(float distance) const {
    return viewDirection_ * distance;
}

Imath::V3f OrbitCamera::pointerRayDirection() const {
    constexpr double pi = 3.14159265358979323846;
    const double projectionScale = std::tan(0.5 * verticalFieldOfViewDegrees * pi / 180.0);
    const double horizontal = horizontalPointerOffset() * aspectRatio() * projectionScale;
    const double vertical = verticalPointerOffset() * projectionScale;
    const Imath::V3d direction = normalized(
        -viewDirectionDouble_ + horizontal * rightDirection() + vertical * upDirectionDouble_);
    return {
        static_cast<float>(direction.x),
        static_cast<float>(direction.y),
        static_cast<float>(direction.z),
    };
}

void OrbitCamera::resetOrbitAnchor() noexcept {
    const Imath::V3d anchorView = normalized(viewDirectionDouble_);
    const Imath::V3d anchorRight = normalized(upDirectionDouble_.cross(anchorView));
    const Imath::V3d anchorUp = normalized(anchorView.cross(anchorRight));
    viewDirectionDouble_ = anchorView;
    upDirectionDouble_ = anchorUp;

    constexpr double orbitRadiansAtViewportEdge = 0.001792;
    const double horizontal = horizontalPointerOffset() * orbitRadiansAtViewportEdge;
    const double vertical = verticalPointerOffset() * orbitRadiansAtViewportEdge;
    if (horizontal == 0.0 && vertical == 0.0) {
        orbitIntegrator_.reset(anchorView, anchorUp, anchorUp, 0);
        syncFromOrbitIntegrator();
        return;
    }

    const Imath::V3d localRight(1.0, 0.0, 0.0);
    const Imath::V3d localUp(0.0, 1.0, 0.0);
    const Imath::V3d localView(0.0, 0.0, 1.0);
    const Imath::V3d yawedRight = rotateAroundAxis(localRight, localUp, horizontal);
    const Imath::V3d yawedView = rotateAroundAxis(localView, localUp, horizontal);
    const Imath::V3d oneStepUp = rotateAroundAxis(localUp, yawedRight, -vertical);
    const Imath::V3d oneStepView = rotateAroundAxis(yawedView, yawedRight, -vertical);
    const double trace = yawedRight.x + oneStepUp.y + oneStepView.z;
    const double requestedStepAngle =
        std::acos(std::clamp((trace - 1.0) * 0.5, -1.0, 1.0));
    if (requestedStepAngle == 0.0) {
        orbitIntegrator_.reset(anchorView, anchorUp, anchorUp, 0);
        syncFromOrbitIntegrator();
        return;
    }

    const Imath::V3d localAxis = normalized({
        oneStepUp.z - oneStepView.y,
        oneStepView.x - yawedRight.z,
        yawedRight.y - oneStepUp.x,
    });
    const Imath::V3d orbitWorldAxis = normalized(
        anchorRight * localAxis.x + anchorUp * localAxis.y + anchorView * localAxis.z);
    const std::uint64_t orbitFramesPerRevolution = std::max<std::uint64_t>(
        1,
        static_cast<std::uint64_t>(std::llround(twoPi / requestedStepAngle)));
    orbitIntegrator_.reset(
        anchorView,
        anchorUp,
        orbitWorldAxis,
        orbitFramesPerRevolution);
    syncFromOrbitIntegrator();
}

void OrbitCamera::syncFromOrbitIntegrator() noexcept {
    viewDirectionDouble_ = orbitIntegrator_.viewDirection();
    upDirectionDouble_ = orbitIntegrator_.upDirection();
    syncFloatDirections();
}

void OrbitCamera::syncFloatDirections() noexcept {
    viewDirection_ = {
        static_cast<float>(viewDirectionDouble_.x),
        static_cast<float>(viewDirectionDouble_.y),
        static_cast<float>(viewDirectionDouble_.z),
    };
    upDirection_ = {
        static_cast<float>(upDirectionDouble_.x),
        static_cast<float>(upDirectionDouble_.y),
        static_cast<float>(upDirectionDouble_.z),
    };
}

Imath::V3d OrbitCamera::rightDirection() const noexcept {
    return normalized(upDirectionDouble_.cross(viewDirectionDouble_));
}

double OrbitCamera::horizontalPointerOffset() const noexcept {
    return 2.0 * static_cast<double>(pointerX_) / static_cast<double>(viewportWidth_) - 1.0;
}

double OrbitCamera::verticalPointerOffset() const noexcept {
    return 1.0 - 2.0 * static_cast<double>(pointerY_) / static_cast<double>(viewportHeight_);
}

}  // namespace hexpuzzle
