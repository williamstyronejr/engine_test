#pragma once
#include "engine/thread_context_switches.hpp"
#include <array>
#include <span>
#include <stdexcept>
#include <vector>

namespace feature_lab {
struct StressContextPhase {
    engine::ThreadContextSwitches before, after;
};
using StressContextPhases = std::array<StressContextPhase, 4>;
// Independent bounded storage leaves all existing timing-row payloads unchanged.
class StressContext {
  public:
    struct Sample {
        std::uint64_t tag{};
        StressContextPhases phases;
    };
    StressContext(std::size_t frames, std::size_t warmup, std::size_t cycles, bool enabled)
        : frames_(frames), warmup_(warmup), cycles_(cycles), enabled_(enabled) {
        if (!frames || frames > 10000 || warmup > 600 || !cycles || cycles > 50 ||
            frames * cycles > 60000)
            throw std::invalid_argument("Invalid context trace bounds");
        if (enabled_)
            samples_.reserve(frames * cycles);
    }
    bool enabled() const { return enabled_; }
    std::size_t buffer_bytes() const { return samples_.capacity() * sizeof(Sample); }
    std::span<const Sample> samples() const { return samples_; }
    void record(std::uint64_t tag, const StressContextPhases& phases) {
        if (!enabled_)
            return;
        const auto index = samples_.size();
        const auto expected =
            (index / frames_) * (frames_ + warmup_) + warmup_ + index % frames_ + 1;
        if (index == frames_ * cycles_ || tag != expected)
            throw std::invalid_argument("Nonsequential or excess context trace");
        auto previous = samples_.empty() ? engine::ThreadContextSwitches{}
                                         : samples_.back().phases.back().after;
        for (const auto& phase : phases) {
            if (phase.before.voluntary < previous.voluntary ||
                phase.before.involuntary < previous.involuntary ||
                phase.after.voluntary < phase.before.voluntary ||
                phase.after.involuntary < phase.before.involuntary)
                throw std::invalid_argument("Regressing thread context-switch counter");
            previous = phase.after;
        }
        samples_.push_back({tag, phases});
    }

  private:
    std::size_t frames_, warmup_, cycles_;
    bool enabled_;
    std::vector<Sample> samples_;
};
} // namespace feature_lab
