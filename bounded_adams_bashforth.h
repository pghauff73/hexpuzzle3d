#pragma once

#include <Imath/ImathVec.h>

#include <cstdint>

namespace hexpuzzle {

class BoundedAdamsBashforthOrbit {
public:
    struct Settings {
        double correctionGain = 0.25;
        double maximumReferenceErrorRadians = 0.000001;
    };

    BoundedAdamsBashforthOrbit();
    explicit BoundedAdamsBashforthOrbit(Settings settings);

    void reset(
        const Imath::V3d& viewDirection,
        const Imath::V3d& upDirection,
        const Imath::V3d& worldAxis,
        std::uint64_t framesPerRevolution) noexcept;
    void update() noexcept;

    bool active() const noexcept;
    const Imath::V3d& viewDirection() const noexcept;
    const Imath::V3d& upDirection() const noexcept;
    std::uint64_t frameIndex() const noexcept;
    std::uint64_t framesPerRevolution() const noexcept;
    double correctionGain() const noexcept;
    double errorBoundRadians() const noexcept;
    double lastPredictionErrorRadians() const noexcept;
    double lastReferenceErrorRadians() const noexcept;
    double maximumReferenceErrorRadians() const noexcept;
    std::uint64_t periodicReanchorCount() const noexcept;
    std::uint64_t safetyReanchorCount() const noexcept;
    bool reanchoredLastUpdate() const noexcept;

private:
    struct Quaternion {
        double w = 1.0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    static Quaternion normalized(Quaternion value) noexcept;
    static Quaternion negated(Quaternion value) noexcept;
    static Quaternion multiplied(const Quaternion& left, const Quaternion& right) noexcept;
    static Quaternion added(const Quaternion& left, const Quaternion& right) noexcept;
    static Quaternion scaled(Quaternion value, double scale) noexcept;
    static double dot(const Quaternion& left, const Quaternion& right) noexcept;
    static Quaternion fromAxisAngle(const Imath::V3d& normalizedAxis, double angle) noexcept;
    static Quaternion fromFrame(
        const Imath::V3d& viewDirection,
        const Imath::V3d& upDirection) noexcept;
    static Imath::V3d rotate(const Quaternion& orientation, const Imath::V3d& value) noexcept;
    static Quaternion slerp(
        const Quaternion& from,
        const Quaternion& to,
        double amount) noexcept;
    static double orientationDistance(
        const Quaternion& left,
        const Quaternion& right) noexcept;

    Quaternion derivative(const Quaternion& orientation) const noexcept;
    Quaternion referenceOrientation(std::uint64_t frameIndex, bool parity) const noexcept;
    Quaternion previousReferenceOrientation(std::uint64_t frameIndex, bool parity) const noexcept;
    void syncDirections() noexcept;

    Settings settings_;
    Quaternion anchorOrientation_;
    Quaternion currentOrientation_;
    Quaternion previousOrientation_;
    Imath::V3d worldAxis_{0.0, 1.0, 0.0};
    Imath::V3d viewDirection_{0.0, 0.0, 1.0};
    Imath::V3d upDirection_{0.0, 1.0, 0.0};
    double stepAngle_ = 0.0;
    double lastPredictionErrorRadians_ = 0.0;
    double lastReferenceErrorRadians_ = 0.0;
    double maximumReferenceErrorRadians_ = 0.0;
    std::uint64_t frameIndex_ = 0;
    std::uint64_t framesPerRevolution_ = 0;
    std::uint64_t periodicReanchorCount_ = 0;
    std::uint64_t safetyReanchorCount_ = 0;
    bool parity_ = false;
    bool reanchoredLastUpdate_ = false;
};

}  // namespace hexpuzzle
