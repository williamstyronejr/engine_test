#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace feature_lab {
struct StressPreparationPhase {
    double wall{}, cpu{}, gl_wall{}, gl_cpu{};
};
using StressPreparationPhases = std::array<StressPreparationPhase, 4>;
// Independent payload preserves existing optional trace rows and their byte accounting.
class StressPreparation {
  public:
    struct Sample {
        std::uint64_t tag{};
        StressPreparationPhases phases;
    };
    StressPreparation(std::size_t frames, std::size_t warmup, std::size_t cycles, bool enabled)
        : frames_(frames), warmup_(warmup), cycles_(cycles), enabled_(enabled) {
        if (!frames || frames > 10000 || warmup > 600 || !cycles || cycles > 50 ||
            frames * cycles > 60000)
            throw std::invalid_argument("Invalid preparation trace bounds");
        if (enabled_)
            samples_.reserve(frames * cycles);
    }
    bool enabled() const { return enabled_; }
    std::size_t buffer_bytes() const { return samples_.capacity() * sizeof(Sample); }
    std::span<const Sample> samples() const { return samples_; }
    void record(std::uint64_t tag, const StressPreparationPhases& phases) {
        if (!enabled_)
            return;
        const auto index = samples_.size();
        const auto expected =
            (index / frames_) * (frames_ + warmup_) + warmup_ + index % frames_ + 1;
        if (index == frames_ * cycles_ || tag != expected)
            throw std::invalid_argument("Nonsequential or excess preparation trace");
        for (const auto& p : phases) {
            for (const auto value : {p.wall, p.cpu, p.gl_wall, p.gl_cpu})
                if (!std::isfinite(value) || value < 0)
                    throw std::invalid_argument("Invalid preparation duration");
            if (p.gl_wall > p.wall || p.gl_cpu > p.cpu)
                throw std::invalid_argument("GL interval exceeds preparation phase");
            // CPU and wall use different clocks/boundaries. Preserve signed differences;
            // do not reject or clamp CPU > wall due to clock/accounting granularity.
        }
        samples_.push_back({tag, phases});
    }

  private:
    std::size_t frames_, warmup_, cycles_;
    bool enabled_;
    std::vector<Sample> samples_;
};
} // namespace feature_lab
