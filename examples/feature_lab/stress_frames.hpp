#pragma once
#include "engine/gpu_timer.hpp"
#include <array>
#include <cmath>
#include <span>
#include <stdexcept>
#include <vector>

namespace feature_lab {
// Optional diagnostic storage. CPU tags include warmup; retained rows exclude it.
// GPU results join by their original frame tag, including across cycle boundaries.
class StressFrames {
  public:
    enum Scope : std::size_t {
        poll_collect,
        simulation,
        mixer,
        render_submit,
        preview_submit,
        present,
        frame,
        scope_count
    };
    struct Sample {
        std::uint64_t tag{};
        std::array<double, scope_count> cpu{};
        double gpu_ms{-1};
        std::uint64_t collected_after{};
        bool submitted{};
    };
    StressFrames(std::size_t frames, std::size_t warmup, std::size_t cycles, bool enabled)
        : frames_(frames), warmup_(warmup), cycles_(cycles), enabled_(enabled) {
        if (!frames || frames > 10000 || warmup > 600 || !cycles || cycles > 50 ||
            frames * cycles > 60000)
            throw std::invalid_argument("Invalid frame trace bounds");
        if (enabled_)
            samples_.reserve(frames * cycles);
    }
    bool enabled() const { return enabled_; }
    std::size_t buffer_bytes() const { return samples_.capacity() * sizeof(Sample); }
    std::span<const Sample> samples() const { return samples_; }
    void record(std::uint64_t tag, const std::array<double, scope_count>& cpu, bool submitted) {
        if (!enabled_)
            return;
        const auto index = samples_.size();
        const auto expected =
            (index / frames_) * (frames_ + warmup_) + warmup_ + index % frames_ + 1;
        if (index == frames_ * cycles_ || tag != expected)
            throw std::invalid_argument("Nonsequential or excess frame trace");
        for (const auto value : cpu)
            if (!std::isfinite(value) || value < 0)
                throw std::invalid_argument("Invalid frame trace duration");
        samples_.push_back({tag, cpu, -1, 0, submitted});
    }
    void collect(engine::GpuTimingSample gpu, std::uint64_t after) {
        if (!enabled_)
            return;
        const auto stride = frames_ + warmup_;
        if (!gpu.frame || gpu.frame > stride * cycles_ || after < gpu.frame ||
            after > stride * cycles_ || !std::isfinite(gpu.milliseconds) || gpu.milliseconds < 0)
            throw std::invalid_argument("Invalid GPU frame trace result");
        const auto step = (gpu.frame - 1) % stride;
        if (step < warmup_)
            return;
        const auto index = (gpu.frame - 1) / stride * frames_ + step - warmup_;
        if (index >= samples_.size() || !samples_[index].submitted || samples_[index].gpu_ms >= 0)
            throw std::invalid_argument("Unrecorded or duplicate GPU frame trace result");
        auto& sample = samples_[index];
        sample.gpu_ms = gpu.milliseconds;
        sample.collected_after = after;
    }

  private:
    std::size_t frames_, warmup_, cycles_;
    bool enabled_;
    std::vector<Sample> samples_;
};
} // namespace feature_lab
