#pragma once

#include <cstddef>
#include <optional>
#include <vector>

namespace hexpuzzle {

class TileSequenceTracker {
public:
    struct Settings {
        std::size_t minimumPeriod = 16;
        std::size_t confirmationCycles = 2;
        std::size_t maximumHistory = 2048;
    };

    TileSequenceTracker();
    explicit TileSequenceTracker(Settings settings);

    void reset();
    void observe(std::size_t tileId);

    bool repeating() const noexcept;
    std::size_t repeatPeriod() const noexcept;
    std::size_t transitionCount() const noexcept;

private:
    void detectRepeat();

    Settings settings_;
    std::optional<std::size_t> lastTile_;
    std::vector<std::size_t> history_;
    std::vector<std::size_t> repeatPattern_;
    std::size_t nextPatternIndex_ = 0;
    std::size_t transitionCount_ = 0;
    bool repeating_ = false;
};

}  // namespace hexpuzzle
