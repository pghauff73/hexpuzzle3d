#include "bounded_adams_bashforth.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hexpuzzle {
namespace {

constexpr double pi = 3.141592653589793238462643383279502884;
constexpr double twoPi = 2.0 * pi;

Imath::V3d normalizedVector(Imath::V3d value) noexcept {
    if (value.length() != 0.0) {
        value.normalize();
    }
    return value;
}

}  // namespace

BoundedAdamsBashforthOrbit::BoundedAdamsBashforthOrbit()
    : BoundedAdamsBashforthOrbit(Settings{}) {
}

BoundedAdamsBashforthOrbit::BoundedAdamsBashforthOrbit(Settings settings)
    : settings_(settings) {
    if (!(settings_.correctionGain > 0.0 && settings_.correctionGain <= 1.0) ||
        !(settings_.maximumReferenceErrorRadians > 0.0 &&
          settings_.maximumReferenceErrorRadians < pi)) {
        throw std::invalid_argument("invalid bounded Adams-Bashforth settings");
    }
}

void BoundedAdamsBashforthOrbit::reset(
    const Imath::V3d& viewDirection,
    const Imath::V3d& upDirection,
    const Imath::V3d& worldAxis,
    std::uint64_t framesPerRevolution) noexcept {
    anchorOrientation_ = fromFrame(viewDirection, upDirection);
    currentOrientation_ = anchorOrientation_;
    worldAxis_ = normalizedVector(worldAxis);
    framesPerRevolution_ = framesPerRevolution;
    frameIndex_ = 0;
    parity_ = false;
    periodicReanchorCount_ = 0;
    safetyReanchorCount_ = 0;
    lastPredictionErrorRadians_ = 0.0;
    lastReferenceErrorRadians_ = 0.0;
    maximumReferenceErrorRadians_ = 0.0;
    reanchoredLastUpdate_ = false;
    if (framesPerRevolution_ == 0) {
        stepAngle_ = 0.0;
        previousOrientation_ = currentOrientation_;
    } else {
        stepAngle_ = twoPi / static_cast<double>(framesPerRevolution_);
        previousOrientation_ = normalized(multiplied(
            fromAxisAngle(worldAxis_, -stepAngle_),
            anchorOrientation_));
    }
    syncDirections();
}

void BoundedAdamsBashforthOrbit::update() noexcept {
    if (!active()) {
        return;
    }

    std::uint64_t nextFrameIndex = frameIndex_ + 1;
    bool nextParity = parity_;
    const bool periodicBoundary = nextFrameIndex == framesPerRevolution_;
    if (periodicBoundary) {
        nextFrameIndex = 0;
        nextParity = !nextParity;
    }

    const Quaternion currentDerivative = derivative(currentOrientation_);
    const Quaternion previousDerivative = derivative(previousOrientation_);
    const Quaternion prediction = normalized(added(
        currentOrientation_,
        added(scaled(currentDerivative, 1.5), scaled(previousDerivative, -0.5))));
    const Quaternion reference = referenceOrientation(nextFrameIndex, nextParity);
    lastPredictionErrorRadians_ = orientationDistance(prediction, reference);
    Quaternion corrected = slerp(prediction, reference, settings_.correctionGain);
    lastReferenceErrorRadians_ = orientationDistance(corrected, reference);

    const bool safetyBoundary =
        lastReferenceErrorRadians_ > settings_.maximumReferenceErrorRadians;
    reanchoredLastUpdate_ = periodicBoundary || safetyBoundary;
    if (reanchoredLastUpdate_) {
        corrected = dot(reference, prediction) < 0.0 ? negated(reference) : reference;
        previousOrientation_ = previousReferenceOrientation(nextFrameIndex, nextParity);
        if (dot(previousOrientation_, corrected) < 0.0) {
            previousOrientation_ = negated(previousOrientation_);
        }
        lastReferenceErrorRadians_ = 0.0;
        if (periodicBoundary) {
            ++periodicReanchorCount_;
        } else {
            ++safetyReanchorCount_;
        }
    } else {
        previousOrientation_ = currentOrientation_;
    }

    currentOrientation_ = normalized(corrected);
    frameIndex_ = nextFrameIndex;
    parity_ = nextParity;
    maximumReferenceErrorRadians_ =
        std::max(maximumReferenceErrorRadians_, lastReferenceErrorRadians_);
    syncDirections();
}

bool BoundedAdamsBashforthOrbit::active() const noexcept {
    return framesPerRevolution_ != 0;
}

const Imath::V3d& BoundedAdamsBashforthOrbit::viewDirection() const noexcept {
    return viewDirection_;
}

const Imath::V3d& BoundedAdamsBashforthOrbit::upDirection() const noexcept {
    return upDirection_;
}

std::uint64_t BoundedAdamsBashforthOrbit::frameIndex() const noexcept {
    return frameIndex_;
}

std::uint64_t BoundedAdamsBashforthOrbit::framesPerRevolution() const noexcept {
    return framesPerRevolution_;
}

double BoundedAdamsBashforthOrbit::correctionGain() const noexcept {
    return settings_.correctionGain;
}

double BoundedAdamsBashforthOrbit::errorBoundRadians() const noexcept {
    return settings_.maximumReferenceErrorRadians;
}

double BoundedAdamsBashforthOrbit::lastPredictionErrorRadians() const noexcept {
    return lastPredictionErrorRadians_;
}

double BoundedAdamsBashforthOrbit::lastReferenceErrorRadians() const noexcept {
    return lastReferenceErrorRadians_;
}

double BoundedAdamsBashforthOrbit::maximumReferenceErrorRadians() const noexcept {
    return maximumReferenceErrorRadians_;
}

std::uint64_t BoundedAdamsBashforthOrbit::periodicReanchorCount() const noexcept {
    return periodicReanchorCount_;
}

std::uint64_t BoundedAdamsBashforthOrbit::safetyReanchorCount() const noexcept {
    return safetyReanchorCount_;
}

bool BoundedAdamsBashforthOrbit::reanchoredLastUpdate() const noexcept {
    return reanchoredLastUpdate_;
}

BoundedAdamsBashforthOrbit::Quaternion BoundedAdamsBashforthOrbit::normalized(
    Quaternion value) noexcept {
    const double length = std::sqrt(dot(value, value));
    if (length != 0.0) {
        value = scaled(value, 1.0 / length);
    }
    return value;
}

BoundedAdamsBashforthOrbit::Quaternion BoundedAdamsBashforthOrbit::negated(
    Quaternion value) noexcept {
    return {-value.w, -value.x, -value.y, -value.z};
}

BoundedAdamsBashforthOrbit::Quaternion BoundedAdamsBashforthOrbit::multiplied(
    const Quaternion& left,
    const Quaternion& right) noexcept {
    return {
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
    };
}

BoundedAdamsBashforthOrbit::Quaternion BoundedAdamsBashforthOrbit::added(
    const Quaternion& left,
    const Quaternion& right) noexcept {
    return {left.w + right.w, left.x + right.x, left.y + right.y, left.z + right.z};
}

BoundedAdamsBashforthOrbit::Quaternion BoundedAdamsBashforthOrbit::scaled(
    Quaternion value,
    double scale) noexcept {
    return {value.w * scale, value.x * scale, value.y * scale, value.z * scale};
}

double BoundedAdamsBashforthOrbit::dot(
    const Quaternion& left,
    const Quaternion& right) noexcept {
    return left.w * right.w + left.x * right.x + left.y * right.y + left.z * right.z;
}

BoundedAdamsBashforthOrbit::Quaternion BoundedAdamsBashforthOrbit::fromAxisAngle(
    const Imath::V3d& normalizedAxis,
    double angle) noexcept {
    const double halfAngle = 0.5 * angle;
    const double sine = std::sin(halfAngle);
    return normalized({
        std::cos(halfAngle),
        normalizedAxis.x * sine,
        normalizedAxis.y * sine,
        normalizedAxis.z * sine,
    });
}

BoundedAdamsBashforthOrbit::Quaternion BoundedAdamsBashforthOrbit::fromFrame(
    const Imath::V3d& viewDirection,
    const Imath::V3d& upDirection) noexcept {
    const Imath::V3d view = normalizedVector(viewDirection);
    const Imath::V3d right = normalizedVector(upDirection.cross(view));
    const Imath::V3d up = normalizedVector(view.cross(right));
    const double m00 = right.x;
    const double m01 = up.x;
    const double m02 = view.x;
    const double m10 = right.y;
    const double m11 = up.y;
    const double m12 = view.y;
    const double m20 = right.z;
    const double m21 = up.z;
    const double m22 = view.z;
    const double trace = m00 + m11 + m22;
    Quaternion result;
    if (trace > 0.0) {
        const double scale = 2.0 * std::sqrt(trace + 1.0);
        result = {
            0.25 * scale,
            (m21 - m12) / scale,
            (m02 - m20) / scale,
            (m10 - m01) / scale,
        };
    } else if (m00 > m11 && m00 > m22) {
        const double scale = 2.0 * std::sqrt(1.0 + m00 - m11 - m22);
        result = {
            (m21 - m12) / scale,
            0.25 * scale,
            (m01 + m10) / scale,
            (m02 + m20) / scale,
        };
    } else if (m11 > m22) {
        const double scale = 2.0 * std::sqrt(1.0 + m11 - m00 - m22);
        result = {
            (m02 - m20) / scale,
            (m01 + m10) / scale,
            0.25 * scale,
            (m12 + m21) / scale,
        };
    } else {
        const double scale = 2.0 * std::sqrt(1.0 + m22 - m00 - m11);
        result = {
            (m10 - m01) / scale,
            (m02 + m20) / scale,
            (m12 + m21) / scale,
            0.25 * scale,
        };
    }
    return normalized(result);
}

Imath::V3d BoundedAdamsBashforthOrbit::rotate(
    const Quaternion& orientation,
    const Imath::V3d& value) noexcept {
    const Quaternion vector{0.0, value.x, value.y, value.z};
    const Quaternion conjugate{
        orientation.w,
        -orientation.x,
        -orientation.y,
        -orientation.z,
    };
    const Quaternion rotated = multiplied(multiplied(orientation, vector), conjugate);
    return {rotated.x, rotated.y, rotated.z};
}

BoundedAdamsBashforthOrbit::Quaternion BoundedAdamsBashforthOrbit::slerp(
    const Quaternion& from,
    const Quaternion& to,
    double amount) noexcept {
    const Quaternion normalizedFrom = normalized(from);
    Quaternion normalizedTo = normalized(to);
    double cosine = dot(normalizedFrom, normalizedTo);
    if (cosine < 0.0) {
        normalizedTo = negated(normalizedTo);
        cosine = -cosine;
    }
    cosine = std::clamp(cosine, 0.0, 1.0);
    if (cosine > 0.9995) {
        return normalized(added(
            scaled(normalizedFrom, 1.0 - amount),
            scaled(normalizedTo, amount)));
    }
    const double angle = std::acos(cosine);
    const double sine = std::sin(angle);
    return normalized(added(
        scaled(normalizedFrom, std::sin((1.0 - amount) * angle) / sine),
        scaled(normalizedTo, std::sin(amount * angle) / sine)));
}

double BoundedAdamsBashforthOrbit::orientationDistance(
    const Quaternion& left,
    const Quaternion& right) noexcept {
    const Quaternion normalizedLeft = normalized(left);
    Quaternion normalizedRight = normalized(right);
    if (dot(normalizedLeft, normalizedRight) < 0.0) {
        normalizedRight = negated(normalizedRight);
    }
    const Quaternion difference = added(normalizedLeft, negated(normalizedRight));
    const double chordLength = std::sqrt(dot(difference, difference));
    return 4.0 * std::asin(std::clamp(0.5 * chordLength, 0.0, 1.0));
}

BoundedAdamsBashforthOrbit::Quaternion BoundedAdamsBashforthOrbit::derivative(
    const Quaternion& orientation) const noexcept {
    const Quaternion angularVelocity{
        0.0,
        worldAxis_.x * stepAngle_,
        worldAxis_.y * stepAngle_,
        worldAxis_.z * stepAngle_,
    };
    return scaled(multiplied(angularVelocity, orientation), 0.5);
}

BoundedAdamsBashforthOrbit::Quaternion BoundedAdamsBashforthOrbit::referenceOrientation(
    std::uint64_t frameIndex,
    bool parity) const noexcept {
    if (frameIndex == 0) {
        return parity ? negated(anchorOrientation_) : anchorOrientation_;
    }
    const double phase = (parity ? twoPi : 0.0) +
        twoPi * static_cast<double>(frameIndex) /
            static_cast<double>(framesPerRevolution_);
    return normalized(multiplied(fromAxisAngle(worldAxis_, phase), anchorOrientation_));
}

BoundedAdamsBashforthOrbit::Quaternion
BoundedAdamsBashforthOrbit::previousReferenceOrientation(
    std::uint64_t frameIndex,
    bool parity) const noexcept {
    const double basePhase = parity ? twoPi : 0.0;
    const double phase = frameIndex == 0
        ? basePhase - stepAngle_
        : basePhase + twoPi * static_cast<double>(frameIndex - 1) /
            static_cast<double>(framesPerRevolution_);
    return normalized(multiplied(fromAxisAngle(worldAxis_, phase), anchorOrientation_));
}

void BoundedAdamsBashforthOrbit::syncDirections() noexcept {
    viewDirection_ = normalizedVector(rotate(currentOrientation_, {0.0, 0.0, 1.0}));
    const Imath::V3d rawUp = normalizedVector(rotate(currentOrientation_, {0.0, 1.0, 0.0}));
    const Imath::V3d right = normalizedVector(rawUp.cross(viewDirection_));
    upDirection_ = normalizedVector(viewDirection_.cross(right));
}

}  // namespace hexpuzzle
