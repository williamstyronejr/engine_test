#pragma once
#include "stress.hpp"
#include <chrono>
#include <string>

namespace feature_lab {
struct StressOptions {
    StressConfig workload;
    std::size_t frames{600}, warmup{60}, cycles{3};
    int width{1280}, height{720};
    bool cpu_only{}, help{};
    std::string report, screenshot, label;
    void validate() const;
};
StressOptions parse_stress_options(int argc, char** argv);
std::string stress_help();
struct Distribution {
    std::size_t samples{};
    double mean{}, median{}, p95{}, p99{}, maximum{};
};
class Measurements {
  public:
    explicit Measurements(std::size_t capacity) : limit_(capacity) { values_.reserve(capacity); }
    void add(double value);
    Distribution summary() const;
    std::size_t buffer_bytes() const { return values_.capacity() * sizeof(double); }

  private:
    std::vector<double> values_;
    std::size_t limit_;
};
std::string json_string(std::string_view text);
using StressClock = std::chrono::steady_clock;
inline double stress_ms(StressClock::time_point start, StressClock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}
struct StressReport {
    explicit StressReport(const StressOptions& options);
    Measurements load, simulation, audio, submission, frame, gpu;
    StressResources resources;
    std::uint64_t checksum{}, quads{}, culled{}, draws{}, tiles{}, pairs{}, contacts{};
    std::size_t completed_cycles{}, measured_frames{}, rss_start{}, rss_end{}, rss_peak{};
    std::size_t texture_peak{}, texture_bytes_peak{}, target_peak{}, target_bytes_peak{};
    std::uint64_t gpu_submitted{}, gpu_completed{}, gpu_skipped{}, gpu_invalid{};
    std::size_t gpu_pending{};
    bool complete{}, resources_stable{true}, gpu_supported{}, vsync_disable_accepted{};
    int window_width{}, window_height{};
    std::string vendor, device, gl_version;
    void observe(const StressArena& arena, const StressResources& baseline);
    void finish_cycle(const StressArena& arena);
    std::string json(const StressOptions& options) const;
    void write(const StressOptions& options) const;
};
std::size_t
stress_rss_bytes(); // Linux resident pages, not allocator-owned bytes; zero if unavailable.
int run_cpu_stress(const StressOptions& options);
int run_stress_mode(int argc, char** argv);
} // namespace feature_lab
