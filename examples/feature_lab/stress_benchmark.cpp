#include "stress_benchmark.hpp"
#include "engine/assets.hpp"
#include "engine/thread_cpu_clock.hpp"
#include "stress_build.hpp"
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <sys/utsname.h>
#include <unistd.h>

namespace feature_lab {
void StressOptions::validate() const {
    workload.validate();
    if (context_samples && !preparation_samples)
        throw std::invalid_argument("Context samples require --preparation-samples");
    if (preparation_samples && !submission_samples)
        throw std::invalid_argument("Preparation samples require --submission-samples");
    if (submission_samples && (cpu_only || !frame_samples))
        throw std::invalid_argument("Submission samples require graphics and --frame-samples");
    if (presentation != StressPresentation::immediate && presentation != StressPresentation::vsync)
        throw std::invalid_argument("Invalid stress presentation mode");
    if (cpu_only && (presentation_explicit || presentation != StressPresentation::immediate))
        throw std::invalid_argument("CPU-only stress has no presentation mode");
    if (frames < 1 || frames > 10000 || warmup > 600 || cycles < 1 || cycles > 50 ||
        frames * cycles > 60000 || width < 64 || height < 64 || width > 4096 || height > 4096)
        throw std::invalid_argument(
            "Stress run outside bounds: frames 1..10000, warmup 0..600, cycles 1..50, at most "
            "60000 measured frames, resolution 64..4096");
    if (label.size() > 120)
        throw std::invalid_argument("Stress label exceeds 120 bytes");
    if (cpu_only && !screenshot.empty())
        throw std::invalid_argument("CPU-only stress cannot capture a screenshot");
}
StressOptions parse_stress_options(int argc, char** argv) {
    StressOptions result;
    bool no_vsync = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        const auto text = [&]() -> std::string_view {
            if (++i == argc)
                throw std::invalid_argument("Missing stress option value");
            return argv[i];
        };
        const auto number = [&]() {
            const auto value = text();
            std::uint64_t parsed{};
            const auto [end, error] =
                std::from_chars(value.data(), value.data() + value.size(), parsed);
            if (error != std::errc{} || end != value.data() + value.size())
                throw std::invalid_argument("Stress options require unsigned decimal integers");
            return parsed;
        };
        if (arg == "--stress" || arg == "--no-audio") {
        } else if (arg == "--no-vsync")
            no_vsync = true;
        else if (arg == "--presentation") {
            if (result.presentation_explicit)
                throw std::invalid_argument("Duplicate stress presentation option");
            const auto mode = text();
            if (mode == "immediate")
                result.presentation = StressPresentation::immediate;
            else if (mode == "vsync")
                result.presentation = StressPresentation::vsync;
            else
                throw std::invalid_argument("Stress presentation must be immediate or vsync");
            result.presentation_explicit = true;
        } else if (arg == "--help")
            result.help = true;
        else if (arg == "--frame-samples")
            result.frame_samples = true;
        else if (arg == "--submission-samples")
            result.submission_samples = true;
        else if (arg == "--preparation-samples")
            result.preparation_samples = true;
        else if (arg == "--context-samples")
            result.context_samples = true;
        else if (arg == "--cpu-only")
            result.cpu_only = true;
        else if (arg == "--dense")
            result.workload.dense = true;
        else if (arg == "--sprites")
            result.workload.sprites = number();
        else if (arg == "--tiles")
            result.workload.tiles = number();
        else if (arg == "--entities")
            result.workload.entities = number();
        else if (arg == "--bodies")
            result.workload.bodies = number();
        else if (arg == "--voices")
            result.workload.voices = number();
        else if (arg == "--seed")
            result.workload.seed = number();
        else if (arg == "--frames")
            result.frames = number();
        else if (arg == "--warmup")
            result.warmup = number();
        else if (arg == "--cycles")
            result.cycles = number();
        else if (arg == "--width" || arg == "--height") {
            const auto value = number();
            if (value > 4096)
                throw std::invalid_argument("Stress resolution exceeds 4096");
            (arg == "--width" ? result.width : result.height) = static_cast<int>(value);
        } else if (arg == "--report")
            result.report = text();
        else if (arg == "--screenshot")
            result.screenshot = text();
        else if (arg == "--label")
            result.label = text();
        else
            throw std::invalid_argument("Unknown/incompatible stress option: " + std::string(arg));
    }
    if (no_vsync && result.presentation == StressPresentation::vsync)
        throw std::invalid_argument("--no-vsync conflicts with --presentation vsync");
    result.validate();
    return result;
}
std::string stress_help() {
    return "Feature Lab stress arena v1: one 60 Hz tick per frame, unpaced by default.\n"
           "--stress [--cpu-only] --sprites 0..65536 --tiles 0..65536 --entities 0..4096\n"
           "--bodies 0..256 [--dense] --voices 0..16 --seed UINT64\n"
           "--frames 1..10000 --warmup 0..600 --cycles 1..50 (at most 60000 measured frames)\n"
           "--width 64..4096 --height 64..4096 --report FILE.json --label TEXT --screenshot "
           "FILE.ppm [--frame-samples] [--presentation immediate|vsync]\n"
           "--submission-samples requires --frame-samples and graphics; CPU driver-call timing.\n"
           "--preparation-samples additionally profiles phases and calling-thread CPU time.\n"
           "--context-samples requires --preparation-samples; counts calling-thread switches.\n"
           "Defaults: 8192 sprites, 4096 tiles, 1024 entities, 128 sparse bodies, 8 voices;\n"
           "600 frames after 60 warmup steps per cycle, 3 cycles, 1280x720 offscreen.\n"
           "Mixer runs offline; no audio device. Presentation requests interval 0 (default) or 1.\n"
           "Swap request issuance does not prove physical synchronization. Escape aborts.\n"
           "CPU mode omits rendering/tile traversal. --no-audio/--no-vsync are accepted aliases.\n";
}
void Measurements::add(double value) {
    if (!std::isfinite(value) || value < 0 || values_.size() == limit_)
        throw std::invalid_argument("Invalid or excess benchmark sample");
    values_.push_back(value);
}
Distribution Measurements::summary() const {
    Distribution result;
    result.samples = values_.size();
    if (values_.empty())
        return result;
    auto sorted = values_;
    std::sort(sorted.begin(), sorted.end());
    long double sum = 0;
    for (const auto value : sorted)
        sum += value;
    result.mean = static_cast<double>(sum / sorted.size());
    result.median = sorted[(sorted.size() - 1) * 50 / 100];
    result.p95 = sorted[(sorted.size() - 1) * 95 / 100];
    result.p99 = sorted[(sorted.size() - 1) * 99 / 100];
    result.maximum = sorted.back();
    return result;
}
std::size_t stress_rss_bytes() {
    std::ifstream input("/proc/self/statm");
    std::size_t size{}, resident{};
    const auto page = sysconf(_SC_PAGESIZE);
    if (!(input >> size >> resident) || page <= 0)
        return 0;
    return resident * static_cast<std::size_t>(page);
}
StressReport::StressReport(const StressOptions& options)
    : load((options.validate(), options.cycles)), simulation(options.frames * options.cycles),
      audio(options.frames * options.cycles), submission(options.frames * options.cycles),
      preview(options.frames * options.cycles), present(options.frames * options.cycles),
      frame(options.frames * options.cycles), gpu(options.frames * options.cycles),
      frame_samples(options.frames, options.warmup, options.cycles, options.frame_samples),
      submission_samples(options.frames, options.warmup, options.cycles,
                         options.submission_samples),
      preparation_samples(options.frames, options.warmup, options.cycles,
                          options.preparation_samples),
      context_samples(options.frames, options.warmup, options.cycles, options.context_samples) {
    if (options.preparation_samples)
        thread_clock_resolution_ns = engine::thread_cpu_resolution_nanoseconds();
    rss_start = rss_peak = stress_rss_bytes();
}
void StressReport::observe(const StressArena& arena, const StressResources& baseline) {
    const auto current = arena.resources();
    resources_stable &= current == baseline;
    if (resources.buffer_bytes)
        resources_stable &= current == resources;
    resources = current;
}
void StressReport::finish_cycle(const StressArena& arena) {
    const auto value = arena.checksum();
    if (completed_cycles && checksum != value)
        throw std::runtime_error("Stress workload changed between identical cycles");
    checksum = value;
    ++completed_cycles;
    rss_peak = std::max(rss_peak, stress_rss_bytes());
}
std::string StressReport::json(const StressOptions& options) const {
    std::string cpu = "unavailable", system = "unavailable", line;
    std::ifstream cpuinfo("/proc/cpuinfo");
    while (std::getline(cpuinfo, line)) {
        if (line.starts_with("model name")) {
            cpu = line.substr(line.find(':') + 1);
            break;
        }
    }
    utsname info{};
    if (uname(&info) == 0)
        system = std::string(info.sysname) + ' ' + info.release + ' ' + info.machine;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(9) << std::boolalpha;
    out << "{\n  \"schema\":\"feature_lab_stress_v1\",\"workload_version\":1,\"mode\":"
        << json_string(options.cpu_only ? "cpu" : "opengl")
        << ",\"label\":" << json_string(options.label) << ",\"complete\":" << complete
        << ",\"resources_stable\":" << resources_stable
        << ",\n  \"build\":{\"revision\":" << json_string(stress_build_revision)
        << ",\"compiler\":" << json_string(stress_build_compiler)
        << ",\"configuration\":" << json_string(stress_build_configuration)
        << ",\"flags\":" << json_string(stress_build_flags)
        << "},\n  \"machine\":{\"cpu\":" << json_string(cpu)
        << ",\"system\":" << json_string(system) << ",\"gl_vendor\":" << json_string(vendor)
        << ",\"gl_device\":" << json_string(device) << ",\"gl_version\":" << json_string(gl_version)
        << "},\n  \"workload\":{\"sprites\":" << options.workload.sprites
        << ",\"tiles\":" << options.workload.tiles << ",\"entities\":" << options.workload.entities
        << ",\"bodies\":" << options.workload.bodies << ",\"voices\":" << options.workload.voices
        << ",\"dense\":" << options.workload.dense
        << ",\"seed\":" << json_string(std::to_string(options.workload.seed))
        << ",\"width\":" << options.width << ",\"height\":" << options.height
        << ",\"warmup_per_cycle\":" << options.warmup << ",\"frames_per_cycle\":" << options.frames
        << ",\"cycles\":" << options.cycles
        << "},\n  \"run\":{\"completed_cycles\":" << completed_cycles
        << ",\"measured_frames\":" << measured_frames
        << ",\"checksum\":" << json_string(std::to_string(checksum))
        << ",\"window_width\":" << window_width << ",\"window_height\":" << window_height
        << ",\"vsync_requested\":"
        << (!options.cpu_only && options.presentation == StressPresentation::vsync)
        << ",\"vsync_disable_accepted\":"
        << (!options.cpu_only && options.presentation == StressPresentation::immediate &&
            presentation_request_issued)
        << ",\"audio_device\":false,\"capture_requested\":" << !options.screenshot.empty()
        << "},\n  \"presentation\":{\"mode\":"
        << json_string(options.cpu_only                                    ? "none"
                       : options.presentation == StressPresentation::vsync ? "vsync"
                                                                           : "immediate")
        << ",\"swap_interval_requested\":"
        << (options.cpu_only                                    ? "null"
            : options.presentation == StressPresentation::vsync ? "1"
                                                                : "0")
        << ",\"request_issued\":" << (!options.cpu_only && presentation_request_issued)
        << ",\"present_calls\":" << present_calls << "},\n  \"totals\":{\"quads\":" << quads
        << ",\"culled\":" << culled << ",\"draws\":" << draws << ",\"visible_tiles\":" << tiles
        << ",\"collision_candidates\":" << pairs << ",\"contacts\":" << contacts
        << "},\n  \"memory\":{\"tracked_cpu_buffer_bytes\":" << resources.buffer_bytes
        << ",\"report_buffer_bytes\":"
        << load.buffer_bytes() + simulation.buffer_bytes() + audio.buffer_bytes() +
               submission.buffer_bytes() + preview.buffer_bytes() + present.buffer_bytes() +
               frame.buffer_bytes() + gpu.buffer_bytes()
        << (frame_samples.enabled()
                ? ",\"frame_sample_buffer_bytes\":" + std::to_string(frame_samples.buffer_bytes())
                : "")
        << (submission_samples.enabled() ? ",\"submission_sample_buffer_bytes\":" +
                                               std::to_string(submission_samples.buffer_bytes())
                                         : "")
        << (preparation_samples.enabled() ? ",\"preparation_sample_buffer_bytes\":" +
                                                std::to_string(preparation_samples.buffer_bytes())
                                          : "")
        << (context_samples.enabled() ? ",\"context_sample_buffer_bytes\":" +
                                            std::to_string(context_samples.buffer_bytes())
                                      : "")
        << ",\"rss_start\":" << rss_start << ",\"rss_end\":" << rss_end
        << ",\"rss_peak_at_cycle_boundary\":" << rss_peak
        << ",\"peak_uploaded_textures\":" << texture_peak
        << ",\"peak_uploaded_texture_bytes\":" << texture_bytes_peak
        << ",\"peak_targets\":" << target_peak << ",\"peak_target_bytes\":" << target_bytes_peak
        << ",\"renderer_baseline\":{\"coverage\":\"owned_buffers_and_texels_v2\",\"observed\":"
        << !options.cpu_only
        << ",\"cpu_vertex_reserved_bytes\":" << renderer_baseline.cpu_vertex_reserved_bytes
        << ",\"cpu_vertex_used_bytes\":" << renderer_baseline.cpu_vertex_used_bytes
        << ",\"gpu_vertex_store_bytes\":" << renderer_baseline.gpu_vertex_store_bytes
        << ",\"gpu_builtin_texture_bytes\":" << renderer_baseline.gpu_builtin_texture_bytes
        << ",\"gpu_uploaded_texture_bytes\":" << renderer_baseline.gpu_uploaded_texture_bytes
        << ",\"gpu_font_texture_bytes\":" << renderer_baseline.gpu_font_texture_bytes
        << ",\"gpu_target_texture_bytes\":" << renderer_baseline.gpu_target_texture_bytes
        << ",\"cpu_font_atlas_reserved_bytes\":" << renderer_baseline.cpu_font_atlas_reserved_bytes
        << ",\"cpu_font_atlas_used_bytes\":" << renderer_baseline.cpu_font_atlas_used_bytes
        << ",\"cpu_font_glyph_reserved_bytes\":" << renderer_baseline.cpu_font_glyph_reserved_bytes
        << ",\"cpu_font_glyph_used_bytes\":" << renderer_baseline.cpu_font_glyph_used_bytes << '}'
        << "},\n  \"gpu_queries\":{\"supported\":" << gpu_supported
        << ",\"method\":" << json_string(engine::gpu_timing_method_name(gpu_method))
        << ",\"counter_bits\":" << gpu_counter_bits << ",\"submitted\":" << gpu_submitted
        << ",\"completed\":" << gpu_completed << ",\"pending\":" << gpu_pending
        << ",\"skipped\":" << gpu_skipped << ",\"invalid\":" << gpu_invalid
        << ",\"failure_capacity\":" << engine::GpuTimer::failure_capacity
        << ",\"failures_omitted\":" << gpu_invalid - gpu_failure_count << ",\"failures\":[";
    for (std::size_t i = 0; i < gpu_failure_count; ++i) {
        const auto& failure = gpu_failures[i];
        out << (i ? "," : "") << "{\"frame\":" << json_string(std::to_string(failure.frame))
            << ",\"start_ns\":" << json_string(std::to_string(failure.start_ns))
            << ",\"end_ns\":" << json_string(std::to_string(failure.end_ns))
            << ",\"last_attempted_frame\":"
            << json_string(std::to_string(failure.last_attempted_frame))
            << ",\"slot\":" << failure.slot << '}';
    }
    out << "]},\n  \"milliseconds\":{";
    const auto metric = [&](std::string_view name, const Measurements& samples, bool comma) {
        const auto s = samples.summary();
        out << (comma ? "," : "") << '\n'
            << "    " << json_string(name) << ":{\"samples\":" << s.samples
            << ",\"mean\":" << s.mean << ",\"median\":" << s.median << ",\"p95\":" << s.p95
            << ",\"p99\":" << s.p99 << ",\"max\":" << s.maximum << '}';
    };
    metric("load", load, false);
    metric("simulation", simulation, true);
    metric("mixer", audio, true);
    metric("render_submit", submission, true);
    metric("preview_submit", preview, true);
    metric("present", present, true);
    metric("frame", frame, true);
    metric("gpu_render", gpu, true);
    out << "\n  }";
    if (frame_samples.enabled()) {
        out << ",\n  \"frame_samples\":{\"schema\":\"stress_frame_samples_v1\","
               "\"cpu_scope_order\":[\"poll_collect\",\"simulation\",\"mixer\","
               "\"render_submit\",\"preview_submit\",\"present\",\"frame\"],\"rows\":[";
        bool comma = false;
        for (const auto& sample : frame_samples.samples()) {
            out << (comma ? "," : "") << "\n{\"frame\":" << json_string(std::to_string(sample.tag))
                << ",\"cpu_ms\":[";
            comma = true;
            for (std::size_t i = 0; i < sample.cpu.size(); ++i)
                out << (i ? "," : "") << sample.cpu[i];
            out << "],\"gpu_status\":"
                << json_string(sample.gpu_ms >= 0 ? "completed"
                               : sample.submitted ? "uncollected"
                                                  : "not_submitted")
                << ",\"gpu_ms\":";
            if (sample.gpu_ms >= 0)
                out << sample.gpu_ms << ",\"gpu_collected_after\":"
                    << json_string(std::to_string(sample.collected_after));
            else
                out << "null,\"gpu_collected_after\":null";
            out << '}';
        }
        out << "]}";
    }
    if (submission_samples.enabled()) {
        out << ",\n  \"submission_samples\":{\"schema\":\"stress_submission_samples_v1\","
               "\"cpu_scope_order\":[\"prepare_other\",\"gl_setup\",\"gl_upload_draw\"],\"rows\":[";
        bool comma = false;
        for (const auto& sample : submission_samples.samples()) {
            out << (comma ? "," : "") << "\n{\"frame\":" << json_string(std::to_string(sample.tag))
                << ",\"cpu_ms\":[" << sample.prepare_other << ',' << sample.setup << ','
                << sample.upload_draw << "],\"render_ms\":" << sample.render
                << ",\"batches\":" << sample.batches << ",\"upload_bytes\":" << sample.upload_bytes
                << '}';
            comma = true;
        }
        out << "]}";
    }
    if (preparation_samples.enabled()) {
        out << ",\n  \"preparation_samples\":{\"schema\":\"stress_preparation_samples_v1\","
               "\"clock\":\"CLOCK_THREAD_CPUTIME_ID\",\"resolution_ns\":"
            << thread_clock_resolution_ns
            << ",\"phase_order\":[\"begin\",\"tiles\",\"sprites\",\"end\"],"
               "\"metric_order\":[\"wall_ms\",\"thread_cpu_ms\",\"gl_wall_ms\",\"gl_thread_cpu_"
               "ms\"],\"rows\":[";
        bool comma = false;
        for (const auto& sample : preparation_samples.samples()) {
            out << (comma ? "," : "") << "\n{\"frame\":" << json_string(std::to_string(sample.tag))
                << ",\"phases\":[";
            comma = true;
            for (std::size_t i = 0; i < sample.phases.size(); ++i) {
                const auto& p = sample.phases[i];
                out << (i ? "," : "") << '[' << p.wall << ',' << p.cpu << ',' << p.gl_wall << ','
                    << p.gl_cpu << ']';
            }
            out << "]}";
        }
        out << "]}";
    }
    if (context_samples.enabled()) {
        out << ",\n  \"context_samples\":{\"schema\":\"stress_context_samples_v1\","
               "\"source\":\"getrusage(RUSAGE_THREAD)\","
               "\"phase_order\":[\"begin\",\"tiles\",\"sprites\",\"end\"],"
               "\"counter_order\":[\"voluntary\",\"involuntary\"],\"rows\":[";
        bool comma = false;
        for (const auto& sample : context_samples.samples()) {
            out << (comma ? "," : "") << "\n{\"frame\":" << json_string(std::to_string(sample.tag))
                << ",\"phases\":[";
            comma = true;
            for (std::size_t i = 0; i < sample.phases.size(); ++i) {
                const auto& p = sample.phases[i];
                out << (i ? "," : "") << "{\"before\":["
                    << json_string(std::to_string(p.before.voluntary)) << ','
                    << json_string(std::to_string(p.before.involuntary)) << "],\"after\":["
                    << json_string(std::to_string(p.after.voluntary)) << ','
                    << json_string(std::to_string(p.after.involuntary)) << "]}";
            }
            out << "]}";
        }
        out << "]}";
    }
    return out.str() + "\n}\n";
}
void StressReport::write(const StressOptions& options) const {
    const auto text = json(options);
    if (!options.report.empty()) {
        const auto path = std::filesystem::absolute(options.report).lexically_normal();
        engine::AssetRoot(path.parent_path()).write_text(path.filename().string(), text);
    }
    std::cout << text;
}
int run_cpu_stress(const StressOptions& requested) {
    auto options = requested;
    options.cpu_only = true;
    options.validate();
    StressReport report(options);
    for (std::size_t cycle = 0; cycle < options.cycles; ++cycle) {
        const auto start = StressClock::now();
        StressArena arena(options.workload);
        report.load.add(stress_ms(start, StressClock::now()));
        const auto baseline = arena.resources();
        for (std::size_t step = 0; step < options.warmup + options.frames; ++step) {
            const auto begin = StressClock::now();
            arena.simulate();
            const auto simulated = StressClock::now();
            arena.mix();
            const auto mixed = StressClock::now();
            if (step >= options.warmup) {
                report.simulation.add(stress_ms(begin, simulated));
                report.audio.add(stress_ms(simulated, mixed));
                report.frame.add(stress_ms(begin, mixed));
                if (options.frame_samples)
                    report.frame_samples.record(
                        cycle * (options.warmup + options.frames) + step + 1,
                        {0, stress_ms(begin, simulated), stress_ms(simulated, mixed), 0, 0, 0,
                         stress_ms(begin, mixed)},
                        false);
                report.pairs += arena.collisions().stats().candidate_pairs;
                report.contacts += arena.collisions().contacts().size();
                ++report.measured_frames;
            }
            report.observe(arena, baseline);
        }
        report.finish_cycle(arena);
    }
    report.rss_end = stress_rss_bytes();
    report.complete = true;
    report.write(options);
    return report.resources_stable ? 0 : 1;
}
} // namespace feature_lab
