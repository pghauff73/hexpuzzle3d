#include "sequence_tracker.h"

#include <algorithm>
#include <stdexcept>

namespace hexpuzzle {

TileSequenceTracker::TileSequenceTracker()
    : TileSequenceTracker(Settings{}) {
}

TileSequenceTracker::TileSequenceTracker(Settings settings)
    : settings_(settings) {
    if (settings_.minimumPeriod == 0 || settings_.confirmationCycles < 2 ||
        settings_.minimumPeriod > settings_.maximumHistory / settings_.confirmationCycles) {
        throw std::invalid_argument("invalid tile sequence tracker settings");
    }
    history_.reserve(settings_.maximumHistory);
}

void TileSequenceTracker::reset() {
    lastTile_.reset();
    history_.clear();
    repeatPattern_.clear();
    nextPatternIndex_ = 0;
    transitionCount_ = 0;
    repeating_ = false;
}

void TileSequenceTracker::observe(std::size_t tileId) {
    if (lastTile_.has_value() && *lastTile_ == tileId) {
        return;
    }
    lastTile_ = tileId;
    ++transitionCount_;
    history_.push_back(tileId);
    if (history_.size() > settings_.maximumHistory) {
        history_.erase(history_.begin());
    }

    if (repeating_) {
        if (tileId == repeatPattern_.at(nextPatternIndex_)) {
            nextPatternIndex_ = (nextPatternIndex_ + 1) % repeatPattern_.size();
            return;
        }
        repeatPattern_.clear();
        nextPatternIndex_ = 0;
        repeating_ = false;
    }

    detectRepeat();
}

bool TileSequenceTracker::repeating() const noexcept {
    return repeating_;
}

std::size_t TileSequenceTracker::repeatPeriod() const noexcept {
    return repeatPattern_.size();
}

std::size_t TileSequenceTracker::transitionCount() const noexcept {
    return transitionCount_;
}

void TileSequenceTracker::detectRepeat() {
    if (history_.size() < settings_.minimumPeriod * settings_.confirmationCycles) {
        return;
    }

    const std::size_t maximumPeriod = history_.size() / settings_.confirmationCycles;
    for (std::size_t period = settings_.minimumPeriod; period <= maximumPeriod; ++period) {
        const std::size_t patternStart =
            history_.size() - period * settings_.confirmationCycles;
        bool matches = true;
        for (std::size_t cycle = 1; cycle < settings_.confirmationCycles; ++cycle) {
            const std::size_t cycleStart = patternStart + cycle * period;
            if (!std::equal(
                    history_.begin() + static_cast<std::ptrdiff_t>(patternStart),
                    history_.begin() + static_cast<std::ptrdiff_t>(patternStart + period),
                    history_.begin() + static_cast<std::ptrdiff_t>(cycleStart))) {
                matches = false;
                break;
            }
        }
        if (!matches) {
            continue;
        }

        repeatPattern_.assign(
            history_.begin() + static_cast<std::ptrdiff_t>(patternStart),
            history_.begin() + static_cast<std::ptrdiff_t>(patternStart + period));
        nextPatternIndex_ = 0;
        repeating_ = true;
        return;
    }
}

}  // namespace hexpuzzle
