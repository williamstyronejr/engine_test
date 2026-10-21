#include "spike_profile.hpp"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace feature_lab {
const char* spike_action_name(SpikeAction action) {
    switch (action) {
    case SpikeAction::shader_reload:
        return "shader_reload";
    case SpikeAction::shader_reject:
        return "shader_reject";
    case SpikeAction::shader_recover:
        return "shader_recover";
    case SpikeAction::texture_reload:
        return "texture_reload";
    case SpikeAction::texture_reject:
        return "texture_reject";
    case SpikeAction::texture_recover:
        return "texture_recover";
    case SpikeAction::font_reload:
        return "font_reload";
    case SpikeAction::transition_reject:
        return "transition_reject";
    case SpikeAction::transition:
        return "transition";
    }
    throw std::invalid_argument("Invalid spike action");
}
void validate_spike_arguments(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--spike-report" || arg == "--assets") {
            if (++i == argc || std::string_view(argv[i]).empty())
                throw std::invalid_argument("Missing spike profile option value");
        } else if (arg != "--no-audio" && arg != "--no-vsync")
            throw std::invalid_argument("Incompatible spike profile option: " + std::string(arg));
    }
}
SpikeProfile::SpikeProfile() {
    frames_.reserve(frame_limit);
    events_.reserve(event_limit);
}
std::optional<SpikeAction> SpikeProfile::next(std::uint64_t frame, bool won) const {
    constexpr std::array actions{SpikeAction::shader_reload,  SpikeAction::shader_reject,
                                 SpikeAction::shader_recover, SpikeAction::texture_reload,
                                 SpikeAction::texture_reject, SpikeAction::texture_recover,
                                 SpikeAction::font_reload};
    if (!frame || frame > frame_limit || failed())
        return {};
    if (frame <= 840 && frame % 120 == 0)
        return actions[static_cast<std::size_t>(frame / 120 - 1)];
    // Delay transitions until the fixed reload sequence has finished.
    if (frame > 840 && won && transition_attempts_ < 3)
        return transition_attempts_ ? SpikeAction::transition : SpikeAction::transition_reject;
    return {};
}
void SpikeProfile::event(SpikeEvent e) {
    if (!e.frame || e.frame != frames_.size() + 1 || events_.size() == event_limit ||
        (!events_.empty() && e.frame <= events_.back().frame) || !std::isfinite(e.milliseconds) ||
        e.milliseconds < 0)
        throw std::invalid_argument("Invalid spike event");
    const auto planned = next(e.frame, true);
    if (!planned || *planned != e.action)
        throw std::invalid_argument("Unexpected spike event order");
    const bool rejection = e.action == SpikeAction::shader_reject ||
                           e.action == SpikeAction::texture_reject ||
                           e.action == SpikeAction::transition_reject;
    const bool transition = e.action == SpikeAction::transition;
    const auto& a = e.before;
    const auto& b = e.after;
    const auto shader_delta =
        e.action == SpikeAction::shader_reload || e.action == SpikeAction::shader_recover;
    const auto texture_delta =
        e.action == SpikeAction::texture_reload || e.action == SpikeAction::texture_recover;
    const auto font_delta = e.action == SpikeAction::font_reload;
    e.verified = e.verified && e.applied == !rejection && a.resources == b.resources &&
                 b.shader == a.shader + shader_delta && b.textures == a.textures + texture_delta &&
                 b.font == a.font + font_delta &&
                 (transition ? (b.level == 1U - a.level && b.hash != a.hash)
                             : (b.level == a.level && b.hash == a.hash)) &&
                 (!rejection || !e.diagnostic.empty()) && e.staging.clean();
    failed_ |= !e.verified;
    if (transition || e.action == SpikeAction::transition_reject)
        ++transition_attempts_;
    if (e.diagnostic.size() > 4096) {
        e.diagnostic.resize(4096);
        e.diagnostic_truncated = true;
    }
    events_.push_back(std::move(e));
}
void SpikeProfile::record(const std::array<double, ReplayProfile::scope_count>& ms,
                          const SpikeResources& resources) {
    if (frames_.size() == frame_limit)
        throw std::invalid_argument("Spike frame limit exceeded");
    for (const auto value : ms)
        if (!std::isfinite(value) || value < 0)
            throw std::invalid_argument("Invalid spike frame timing");
    if (frames_.size() == warmup)
        baseline_ = resources;
    if (frames_.size() >= warmup)
        resources_stable_ &= resources == baseline_;
    frames_.push_back({ms, {}});
}
void SpikeProfile::collect_gpu(std::span<const engine::GpuTimingSample> samples) {
    for (const auto& sample : samples) {
        if (!sample.frame || sample.frame <= last_gpu_frame_ || sample.frame > frames_.size() ||
            !std::isfinite(sample.milliseconds) || sample.milliseconds < 0)
            throw std::invalid_argument("Invalid spike GPU sample");
        frames_[static_cast<std::size_t>(sample.frame - 1)].gpu = sample.milliseconds;
        last_gpu_frame_ = sample.frame;
    }
}
void SpikeProfile::observe_collision(unsigned level, const engine::CollisionWorld& world) {
    if (level >= collision_usage_.size())
        throw std::invalid_argument("Invalid collision profile level");
    const auto usage = world.memory_usage();
    auto& saved = collision_usage_[level];
    if (!saved) {
        saved = usage;
        return;
    }
    for (std::size_t i = 0; i < usage.groups.size(); ++i) {
        saved->groups[i].reserved_bytes =
            std::max(saved->groups[i].reserved_bytes, usage.groups[i].reserved_bytes);
        saved->groups[i].peak_entries =
            std::max(saved->groups[i].peak_entries, usage.groups[i].peak_entries);
    }
    saved->fixed_bytes = std::max(saved->fixed_bytes, usage.fixed_bytes);
}
void SpikeProfile::observe_scene(unsigned level, const engine::Scene& scene) {
    if (level >= scene_usage_.size())
        throw std::invalid_argument("Invalid scene profile level");
    const auto usage = scene.memory_usage();
    auto& saved = scene_usage_[level];
    if (!saved) {
        saved = usage;
        return;
    }
    const auto merge = [](std::size_t& a, std::size_t b) { a = std::max(a, b); };
    for (std::size_t i = 0; i < usage.core.size(); ++i) {
        merge(saved->core[i].reserved_bytes, usage.core[i].reserved_bytes);
        merge(saved->core[i].peak_entries, usage.core[i].peak_entries);
    }
    auto& a = saved->components;
    const auto& b = usage.components;
    merge(a.pools, b.pools);
    merge(a.dense_reserved_bytes, b.dense_reserved_bytes);
    merge(a.sparse_reserved_bytes, b.sparse_reserved_bytes);
    merge(a.dense_capacity_entries, b.dense_capacity_entries);
    merge(a.sparse_capacity_entries, b.sparse_capacity_entries);
    merge(a.peak_entries_sum, b.peak_entries_sum);
    merge(a.peak_dense_bytes_sum, b.peak_dense_bytes_sum);
}
std::string SpikeProfile::json() const {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9) << std::boolalpha;
    const auto state = [&](const SpikeState& s) {
        out << "{\"hash\":" << json_string(std::to_string(s.hash)) << ",\"level\":" << s.level
            << ",\"shader_revision\":" << s.shader << ",\"texture_revision\":" << s.textures
            << ",\"font_revision\":" << s.font << ",\"textures\":" << s.resources.textures
            << ",\"texture_bytes\":" << s.resources.texture_bytes
            << ",\"targets\":" << s.resources.targets
            << ",\"target_bytes\":" << s.resources.target_bytes
            << ",\"font_bytes\":" << s.resources.font_bytes << '}';
    };
    out << "{\"complete\":" << complete() << ",\"resources_stable\":" << resources_stable_
        << ",\"expected_events\":" << event_limit
        << ",\"cpu_scope_order\":[\"simulation\",\"minimap_submit\",\"world_submit\",\"ui_submit\","
           "\"present\",\"frame\"]"
        << ",\"events\":[";
    for (std::size_t i = 0; i < events_.size(); ++i) {
        const auto& e = events_[i];
        out << (i ? "," : "") << "\n{\"frame\":" << e.frame
            << ",\"action\":" << json_string(spike_action_name(e.action))
            << ",\"operation_ms\":" << e.milliseconds << ",\"applied\":" << e.applied
            << ",\"verified\":" << e.verified
            << ",\"diagnostic_truncated\":" << e.diagnostic_truncated
            << ",\"diagnostic\":" << json_string(e.diagnostic) << ",\"before\":";
        state(e.before);
        out << ",\"after\":";
        state(e.after);
        out << ",\"staging\":{\"coverage\":\"owned_payload_v2\",\"measured\":" << e.staging.measured
            << ",\"cpu_peak_bytes\":" << e.staging.cpu_peak
            << ",\"gpu_peak_bytes\":" << e.staging.gpu_peak
            << ",\"combined_peak_bytes\":" << e.staging.combined_peak
            << ",\"cpu_remaining_bytes\":" << e.staging.cpu_remaining
            << ",\"gpu_remaining_bytes\":" << e.staging.gpu_remaining << '}';
        out << ",\"nearby_frames\":[";
        const auto first = e.frame > 2 ? e.frame - 2 : 1;
        const auto last = std::min<std::uint64_t>(e.frame + 2, frames_.size());
        for (auto frame = first; frame <= last; ++frame) {
            const auto& f = frames_[static_cast<std::size_t>(frame - 1)];
            out << (frame == first ? "" : ",") << "{\"frame\":" << frame << ",\"cpu_ms\":[";
            for (std::size_t scope = 0; scope < f.cpu.size(); ++scope)
                out << (scope ? "," : "") << f.cpu[scope];
            out << "],\"gpu_render_ms\":";
            if (f.gpu)
                out << *f.gpu;
            else
                out << "null";
            out << '}';
        }
        out << "]}";
    }
    out << "],\"collision_memory\":{\"coverage\":\"successful_snapshots_v1\",\"levels\":[";
    bool first_level = true;
    for (std::size_t level = 0; level < collision_usage_.size(); ++level) {
        if (!collision_usage_[level])
            continue;
        const auto& usage = *collision_usage_[level];
        out << (first_level ? "" : ",") << "{\"level\":" << level
            << ",\"reserved_bytes\":" << usage.reserved_bytes()
            << ",\"fixed_bytes\":" << usage.fixed_bytes << ",\"groups\":{";
        first_level = false;
        for (std::size_t i = 0; i < usage.groups.size(); ++i) {
            const auto& group = usage.groups[i];
            out << (i ? "," : "") << json_string(engine::CollisionMemoryUsage::names[i])
                << ":{\"reserved_bytes\":" << group.reserved_bytes
                << ",\"entry_bytes\":" << group.entry_bytes << ",\"buffers\":" << group.buffers
                << ",\"peak_entries\":" << group.peak_entries << '}';
        }
        out << "}}";
    }
    out << "]},\"scene_memory\":{\"coverage\":\"successful_mutations_v1\",\"levels\":[";
    first_level = true;
    for (std::size_t level = 0; level < scene_usage_.size(); ++level) {
        if (!scene_usage_[level])
            continue;
        const auto& usage = *scene_usage_[level];
        out << (first_level ? "" : ",") << "{\"level\":" << level
            << ",\"reserved_bytes\":" << usage.reserved_bytes() << ",\"core\":{";
        first_level = false;
        for (std::size_t i = 0; i < usage.core.size(); ++i) {
            const auto& group = usage.core[i];
            out << (i ? "," : "") << json_string(engine::SceneMemoryUsage::names[i])
                << ":{\"reserved_bytes\":" << group.reserved_bytes
                << ",\"entry_bytes\":" << group.entry_bytes
                << ",\"peak_entries\":" << group.peak_entries << '}';
        }
        const auto& c = usage.components;
        out << "},\"components\":{\"pools\":" << c.pools
            << ",\"dense_reserved_bytes\":" << c.dense_reserved_bytes
            << ",\"sparse_reserved_bytes\":" << c.sparse_reserved_bytes
            << ",\"dense_capacity_entries\":" << c.dense_capacity_entries
            << ",\"sparse_capacity_entries\":" << c.sparse_capacity_entries
            << ",\"peak_entries_sum\":" << c.peak_entries_sum
            << ",\"peak_dense_bytes_sum\":" << c.peak_dense_bytes_sum << "}}";
    }
    return out.str() + "]}}";
}
} // namespace feature_lab
