#include "replay_profile.hpp"
#include "spike_profile.hpp"
#include "engine/assets.hpp"
#include "stress_build.hpp"
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <sys/utsname.h>

namespace feature_lab {
namespace {
std::size_t checked_capacity(std::size_t commands, std::size_t warmup) {
    if (commands == 0 || commands > 36000 || warmup >= commands)
        throw std::invalid_argument(
            "Replay profile requires 1..36000 commands and warmup < commands");
    return commands;
}
} // namespace
void validate_profile_arguments(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--replay" || arg == "--assets" || arg == "--profile-report" ||
            arg == "--profile-warmup") {
            if (++i == argc || std::string_view(argv[i]).empty())
                throw std::invalid_argument("Missing replay profile option value");
        } else if (arg != "--no-audio" && arg != "--no-vsync")
            throw std::invalid_argument("Incompatible replay profile option: " + std::string(arg));
    }
}
std::uint64_t replay_fingerprint(std::span<const std::uint8_t> bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto byte : bytes)
        hash = (hash ^ byte) * 1099511628211ULL;
    return hash;
}
ReplayProfile::ReplayProfile(std::size_t commands, std::size_t warmup, std::uint64_t fingerprint)
    : commands_(checked_capacity(commands, warmup)), warmup_(warmup), fingerprint_(fingerprint),
      timings_{Measurements(commands_), Measurements(commands_), Measurements(commands_),
               Measurements(commands_), Measurements(commands_), Measurements(commands_)},
      gpu_(commands_) {
}
void ReplayProfile::record(const std::array<double, scope_count>& ms, int width, int height,
                           const engine::RenderStats& window, const engine::RenderStats& map,
                           unsigned simulation_steps) {
    if (frames_ == commands_ || width <= 0 || height <= 0 || simulation_steps > 1)
        throw std::invalid_argument("Invalid replay profile frame");
    for (const auto value : ms)
        if (!std::isfinite(value) || value < 0)
            throw std::invalid_argument("Invalid replay profile timing");
    if (!frames_ || frames_ == warmup_) {
        width_ = width;
        height_ = height;
    }
    if (frames_ > warmup_)
        resized_ |= width != width_ || height != height_;
    if (++frames_ <= warmup_)
        return;
    for (std::size_t i = 0; i < scope_count; ++i)
        timings_[i].add(ms[i]);
    quads_ += window.quads;
    draws_ += window.draws;
    map_quads_ += map.quads;
    map_draws_ += map.draws;
    simulation_steps_ += simulation_steps;
}
void ReplayProfile::collect_gpu(std::span<const engine::GpuTimingSample> samples) {
    for (const auto& sample : samples) {
        if (!sample.frame || sample.frame <= last_gpu_frame_ || sample.frame > commands_ ||
            !std::isfinite(sample.milliseconds) || sample.milliseconds < 0)
            throw std::invalid_argument("Invalid replay profile GPU sample");
        last_gpu_frame_ = sample.frame;
        if (sample.frame > warmup_)
            gpu_.add(sample.milliseconds);
    }
}
std::string ReplayProfile::json(std::uint64_t final_hash, std::uint64_t ticks,
                                std::string_view level, const engine::GraphicsInfo& device,
                                bool vsync_disabled, const engine::GpuTimingStats& gpu,
                                std::span<const engine::GpuTimingFailure> failures,
                                const SpikeProfile* spikes) const {
    std::string cpu = "unavailable", system = "unavailable", line;
    std::ifstream input("/proc/cpuinfo");
    while (std::getline(input, line))
        if (line.starts_with("model name")) {
            cpu = line.substr(line.find(':') + 1);
            break;
        }
    utsname info{};
    if (uname(&info) == 0)
        system = std::string(info.sysname) + ' ' + info.release + ' ' + info.machine;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9) << std::boolalpha;
    out << "{\"schema\":"
        << json_string(spikes ? "feature_lab_spike_profile_v1" : "feature_lab_replay_profile_v1")
        << ",\"build\":{\"revision\":"
        << json_string(stress_build_revision)
        << ",\"compiler\":" << json_string(stress_build_compiler)
        << ",\"configuration\":" << json_string(stress_build_configuration)
        << ",\"flags\":" << json_string(stress_build_flags)
        << "},\"machine\":{\"cpu\":" << json_string(cpu) << ",\"system\":" << json_string(system)
        << ",\"gl_vendor\":" << json_string(device.vendor)
        << ",\"gl_device\":" << json_string(device.device)
        << ",\"gl_version\":" << json_string(device.version)
        << "},\"workload\":{";
    if (spikes)
        out << "\"name\":\"reload_transition_v1\",\"seed\":\"1\",\"frame_limit\":" << commands_;
    else
        out << "\"replay_fingerprint\":" << json_string(std::to_string(fingerprint_))
            << ",\"commands\":" << commands_;
    out << ",\"warmup\":" << warmup_
        << ",\"level\":" << json_string(level) << "},\"run\":{\"complete\":" << (complete() && (!spikes || spikes->complete()))
        << ",\"frames\":" << frames_
        << ",\"measured_frames\":" << (frames_ > warmup_ ? frames_ - warmup_ : 0)
        << ",\"final_hash\":" << json_string(std::to_string(final_hash)) << ",\"ticks\":" << ticks
        << ",\"width\":" << width_ << ",\"height\":" << height_ << ",\"resized\":" << resized_
        << ",\"audio_device\":false,\"vsync_requested\":false,\"vsync_disable_accepted\":"
        << vsync_disabled << "},\"totals\":{\"window_quads\":" << quads_
        << ",\"window_draws\":" << draws_ << ",\"minimap_quads\":" << map_quads_
        << ",\"minimap_draws\":" << map_draws_ << ",\"simulation_steps\":" << simulation_steps_
        << "},\"gpu_queries\":{\"supported\":" << gpu.supported
        << ",\"method\":" << json_string(engine::gpu_timing_method_name(gpu.method))
        << ",\"counter_bits\":" << gpu.counter_bits
        << ",\"submitted\":" << gpu.submitted << ",\"completed\":" << gpu.completed
        << ",\"pending\":" << gpu.pending << ",\"skipped\":" << gpu.skipped
        << ",\"invalid\":" << gpu.invalid
        << ",\"failure_capacity\":" << engine::GpuTimer::failure_capacity
        << ",\"failures_omitted\":" << gpu.invalid - failures.size() << ",\"failures\":[";
    for (std::size_t i = 0; i < failures.size(); ++i) {
        const auto& f = failures[i];
        out << (i ? "," : "") << "{\"frame\":" << json_string(std::to_string(f.frame))
            << ",\"start_ns\":" << json_string(std::to_string(f.start_ns))
            << ",\"end_ns\":" << json_string(std::to_string(f.end_ns))
            << ",\"last_attempted_frame\":" << json_string(std::to_string(f.last_attempted_frame))
            << ",\"slot\":" << f.slot << '}';
    }
    out << "]},\"milliseconds\":{";
    constexpr std::array names{"simulation", "minimap_submit", "world_submit", "ui_submit",
                               "present",    "frame",          "gpu_render"};
    for (std::size_t i = 0; i <= scope_count; ++i) {
        const auto s = (i == scope_count ? gpu_ : timings_[i]).summary();
        out << (i ? "," : "") << '\n'
            << json_string(names[i]) << ":{\"samples\":" << s.samples << ",\"mean\":" << s.mean
            << ",\"median\":" << s.median << ",\"p95\":" << s.p95 << ",\"p99\":" << s.p99
            << ",\"max\":" << s.maximum << '}';
    }
    out << "\n}";
    if (spikes)
        out << ",\"spikes\":" << spikes->json();
    return out.str() + "}\n";
}
void ReplayProfile::write(std::string_view path, std::string_view json) const {
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    engine::AssetRoot(absolute.parent_path()).write_text(absolute.filename().string(), json);
}
} // namespace feature_lab
