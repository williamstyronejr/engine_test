#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

namespace engine {
struct GpuTimingSample {
    std::uint64_t frame{};
    double milliseconds{};
};
struct GpuTimingStats {
    int counter_bits{};
    bool supported{};
    std::size_t pending{};
    std::uint64_t submitted{}, completed{}, skipped{}, invalid{};
};
// Owning GL context/thread only, including construction/destruction. No CPU waits.
class GpuTimer {
  public:
    static constexpr std::size_t capacity = 8;
    GpuTimer();
    ~GpuTimer();
    GpuTimer(const GpuTimer&) = delete;
    GpuTimer& operator=(const GpuTimer&) = delete;
    // Tags must be nonzero and strictly increasing. Always pair begin/end, even if false.
    // False means unsupported counters or a full pending pool; rendering continues normally.
    bool begin(std::uint64_t frame);
    void end();
    // Between scopes only. Poll once; never wait or read an unavailable result.
    // Returned span remains valid until the next collect or destruction.
    std::span<const GpuTimingSample> collect();
    GpuTimingStats stats() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace engine
