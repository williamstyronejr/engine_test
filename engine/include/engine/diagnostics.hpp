#pragma once
#include "engine/gpu_timer.hpp"
#include "engine/input.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace engine {
struct FrameSample {
    double wall_ms{}, cpu_ms{};
};
struct FrameSummary {
    std::size_t samples{};
    double wall_mean_ms{}, cpu_mean_ms{}, cpu_p95_ms{}, cpu_max_ms{};
};
// Rolling history, with no allocation. CPU duration excludes presentation/readback;
// wall duration is the interval between frame starts, including pacing.
class FrameHistory {
  public:
    static constexpr std::size_t capacity = 240;
    void record(FrameSample sample) {
        if (!std::isfinite(sample.wall_ms) || !std::isfinite(sample.cpu_ms) || sample.wall_ms < 0 ||
            sample.cpu_ms < 0)
            throw std::invalid_argument("Invalid diagnostic frame duration");
        samples_[next_] = sample;
        next_ = (next_ + 1) % capacity;
        count_ = std::min(count_ + 1, capacity);
    }
    FrameSummary summary() const {
        FrameSummary result;
        result.samples = count_;
        if (!count_)
            return result;
        std::array<double, capacity> cpu{};
        // Long double accumulation also avoids intermediate double overflow.
        long double wall_sum = 0, cpu_sum = 0;
        for (std::size_t i = 0; i < count_; ++i) {
            wall_sum += samples_[i].wall_ms;
            cpu_sum += samples_[i].cpu_ms;
            cpu[i] = samples_[i].cpu_ms;
        }
        result.wall_mean_ms = static_cast<double>(wall_sum / count_);
        result.cpu_mean_ms = static_cast<double>(cpu_sum / count_);
        std::sort(cpu.begin(), cpu.begin() + static_cast<std::ptrdiff_t>(count_));
        result.cpu_p95_ms = cpu[(count_ - 1) * 95 / 100];
        result.cpu_max_ms = cpu[count_ - 1];
        return result;
    }
    void clear() { next_ = count_ = 0; }

  private:
    std::array<FrameSample, capacity> samples_{};
    std::size_t next_{}, count_{};
};
struct GpuTimingSummary {
    std::size_t samples{};
    std::uint64_t last_frame{};
    double last_ms{}, mean_ms{}, p95_ms{}, max_ms{};
};
// Separate history: delayed GPU samples must never be paired with the current CPU frame.
class GpuTimingHistory {
  public:
    static constexpr std::size_t capacity = 240;
    void record(GpuTimingSample sample) {
        if (!sample.frame || sample.frame <= last_.frame || !std::isfinite(sample.milliseconds) ||
            sample.milliseconds < 0)
            throw std::invalid_argument("Invalid or unordered GPU timing sample");
        values_[next_] = sample.milliseconds;
        next_ = (next_ + 1) % capacity;
        count_ = std::min(count_ + 1, capacity);
        last_ = sample;
    }
    GpuTimingSummary summary() const {
        GpuTimingSummary result{count_, last_.frame, last_.milliseconds};
        if (!count_)
            return result;
        auto sorted = values_;
        long double sum = 0;
        for (std::size_t i = 0; i < count_; ++i)
            sum += values_[i];
        result.mean_ms = static_cast<double>(sum / count_);
        std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(count_));
        result.p95_ms = sorted[(count_ - 1) * 95 / 100];
        result.max_ms = sorted[count_ - 1];
        return result;
    }
    void clear() {
        next_ = count_ = 0;
        last_ = {};
    }

  private:
    std::array<double, capacity> values_{};
    GpuTimingSample last_;
    std::size_t next_{}, count_{};
};
struct DebugControls {
    bool visible{}, shapes{};
    void update(const InputFrame& input) {
        if (button(input, Key::diagnostics).pressed)
            visible = !visible;
        if (button(input, Key::debug_shapes).pressed)
            shapes = !shapes;
    }
};
} // namespace engine
