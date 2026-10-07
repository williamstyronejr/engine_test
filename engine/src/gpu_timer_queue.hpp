#pragma once
#include "engine/gpu_timer.hpp"
#include <array>
#include <stdexcept>

namespace engine::detail {
// Backend supplies stamp(index), available(index), and result(index). The same
// bounded scheduling code runs against GL and deterministic delayed-result tests.
template <class Backend> class GpuTimerQueue {
  public:
    GpuTimerQueue(Backend& backend, int counter_bits) : backend_(backend) {
        stats_.counter_bits = counter_bits;
        // Narrow counters can overflow during a slow frame. Report unavailable instead.
        stats_.supported = counter_bits == 64;
    }
    GpuTimerQueue(const GpuTimerQueue&) = delete;
    GpuTimerQueue& operator=(const GpuTimerQueue&) = delete;
    bool begin(std::uint64_t frame) {
        if (open_)
            throw std::logic_error("GPU timer scope already open");
        if (!frame || frame <= last_frame_)
            throw std::invalid_argument("GPU timer frame tags must increase from 1");
        last_frame_ = frame;
        open_ = true;
        measuring_ = stats_.supported && stats_.pending < GpuTimer::capacity;
        if (!measuring_) {
            ++stats_.skipped;
            return false;
        }
        const auto slot = (head_ + stats_.pending) % GpuTimer::capacity;
        frames_[slot] = frame;
        backend_.stamp(slot * 2);
        return true;
    }
    void end() {
        if (!open_)
            throw std::logic_error("GPU timer end without begin");
        if (measuring_) {
            const auto slot = (head_ + stats_.pending) % GpuTimer::capacity;
            backend_.stamp(slot * 2 + 1);
            ++stats_.pending;
            ++stats_.submitted;
        }
        open_ = measuring_ = false;
    }
    std::span<const GpuTimingSample> collect() {
        if (open_)
            throw std::logic_error("GPU timer collect inside scope");
        std::size_t count = 0;
        while (stats_.pending) {
            const auto start = head_ * 2, end = start + 1;
            // Readiness checks are bounded to this pool. An unavailable head stops polling.
            if (!backend_.available(end) || !backend_.available(start))
                break;
            const auto a = backend_.result(start), b = backend_.result(end);
            if (b < a)
                ++stats_.invalid; // Clock reset/wrap: discard, never publish a huge duration.
            else {
                completed_[count++] = {frames_[head_], static_cast<double>(b - a) / 1000000.0};
                ++stats_.completed;
            }
            head_ = (head_ + 1) % GpuTimer::capacity;
            --stats_.pending;
        }
        return {completed_.data(), count};
    }
    GpuTimingStats stats() const { return stats_; }

  private:
    Backend& backend_;
    GpuTimingStats stats_;
    std::array<std::uint64_t, GpuTimer::capacity> frames_{};
    std::array<GpuTimingSample, GpuTimer::capacity> completed_{};
    std::size_t head_{};
    std::uint64_t last_frame_{};
    bool open_{}, measuring_{};
};
} // namespace engine::detail
