#pragma once
#include "engine/renderer.hpp"
#include "stress_benchmark.hpp"

namespace feature_lab {
// One checked replay command per rendered frame. Warmup is a prefix of the replay.
class ReplayProfile {
  public:
    enum Scope : std::size_t { simulation, minimap, world, ui, present, frame, scope_count };
    ReplayProfile(std::size_t commands, std::size_t warmup, std::uint64_t fingerprint);
    void record(const std::array<double, scope_count>& milliseconds, int width, int height,
                const engine::RenderStats& window, const engine::RenderStats& map,
                unsigned simulation_steps);
    void collect_gpu(std::span<const engine::GpuTimingSample> samples);
    bool complete() const { return frames_ == commands_ && !resized_; }
    std::string json(std::uint64_t final_hash, std::uint64_t ticks, std::string_view level,
                     const engine::GraphicsInfo& device, bool vsync_disabled,
                     const engine::GpuTimingStats& gpu,
                     std::span<const engine::GpuTimingFailure> failures) const;
    void write(std::string_view path, std::string_view json) const;

  private:
    std::size_t commands_, warmup_, frames_{};
    std::uint64_t fingerprint_, last_gpu_frame_{};
    std::array<Measurements, scope_count> timings_;
    Measurements gpu_;
    int width_{}, height_{};
    bool resized_{};
    std::uint64_t quads_{}, draws_{}, map_quads_{}, map_draws_{}, simulation_steps_{};
};
void validate_profile_arguments(int argc, char** argv);
std::uint64_t replay_fingerprint(std::span<const std::uint8_t> bytes);
} // namespace feature_lab
