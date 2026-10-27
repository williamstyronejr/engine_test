#pragma once
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace feature_lab {
// Separate optional storage keeps the existing frame-row ABI and disabled payload unchanged.
class StressSubmission {
  public:
    struct Sample {
        std::uint64_t tag{};
        double prepare_other{}, setup{}, upload_draw{}, render{};
        std::size_t batches{}, upload_bytes{};
    };
    StressSubmission(std::size_t frames, std::size_t warmup, std::size_t cycles, bool enabled)
        : frames_(frames), warmup_(warmup), cycles_(cycles), enabled_(enabled) {
        if (!frames || frames > 10000 || warmup > 600 || !cycles || cycles > 50 ||
            frames * cycles > 60000)
            throw std::invalid_argument("Invalid submission trace bounds");
        if (enabled_)
            samples_.reserve(frames * cycles);
    }
    bool enabled() const { return enabled_; }
    std::size_t buffer_bytes() const { return samples_.capacity() * sizeof(Sample); }
    std::span<const Sample> samples() const { return samples_; }
    void record(std::uint64_t tag, double render, double setup, double upload_draw,
                std::size_t batches, std::size_t upload_bytes) {
        if (!enabled_)
            return;
        const auto index = samples_.size();
        const auto expected =
            (index / frames_) * (frames_ + warmup_) + warmup_ + index % frames_ + 1;
        if (index == frames_ * cycles_ || tag != expected)
            throw std::invalid_argument("Nonsequential or excess submission trace");
        for (const auto value : {render, setup, upload_draw})
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("Invalid submission duration");
        // Nested steady-clock intervals cannot exceed their enclosing render interval.
        if (setup + upload_draw > render || (batches == 0) != (upload_bytes == 0))
            throw std::invalid_argument("Inconsistent submission trace");
        samples_.push_back({tag, render - (setup + upload_draw), setup, upload_draw, render,
                            batches, upload_bytes});
    }

  private:
    std::size_t frames_, warmup_, cycles_;
    bool enabled_;
    std::vector<Sample> samples_;
};
} // namespace feature_lab
