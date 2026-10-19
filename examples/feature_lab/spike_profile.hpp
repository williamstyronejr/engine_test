#pragma once
#include "replay_profile.hpp"
#include "engine/staging_memory.hpp"
#include <optional>

namespace feature_lab {
enum class SpikeAction {
    shader_reload,
    shader_reject,
    shader_recover,
    texture_reload,
    texture_reject,
    texture_recover,
    font_reload,
    transition_reject,
    transition
};
const char* spike_action_name(SpikeAction action);
struct SpikeResources {
    std::size_t textures{}, texture_bytes{}, targets{}, target_bytes{}, font_bytes{};
    bool operator==(const SpikeResources&) const = default;
};
struct SpikeState {
    std::uint64_t hash{}, shader{}, textures{}, font{};
    unsigned level{};
    SpikeResources resources;
};
struct SpikeEvent {
    std::uint64_t frame{};
    SpikeAction action{};
    double milliseconds{};
    bool applied{}, verified{};
    SpikeState before, after;
    std::string diagnostic;
    bool diagnostic_truncated{};
    engine::StagingSnapshot staging{};
};
// Fixed, versioned workload. Diagnostics never write or corrupt asset files.
class SpikeProfile {
  public:
    static constexpr std::size_t frame_limit = 3600, warmup = 30, event_limit = 10;
    SpikeProfile();
    std::optional<SpikeAction> next(std::uint64_t frame, bool won) const;
    void event(SpikeEvent event);
    void record(const std::array<double, ReplayProfile::scope_count>& ms,
                const SpikeResources& resources);
    void collect_gpu(std::span<const engine::GpuTimingSample> samples);
    bool failed() const { return failed_ || !resources_stable_; }
    bool complete() const {
        return frames_.size() == frame_limit && events_.size() == event_limit && !failed();
    }
    std::string json() const;

  private:
    struct Frame {
        std::array<double, ReplayProfile::scope_count> cpu;
        std::optional<double> gpu;
    };
    std::vector<Frame> frames_;
    std::vector<SpikeEvent> events_;
    SpikeResources baseline_{};
    std::uint64_t last_gpu_frame_{};
    unsigned transition_attempts_{};
    bool failed_{}, resources_stable_{true};
};
void validate_spike_arguments(int argc, char** argv);
} // namespace feature_lab
