#include "stress_benchmark.hpp"
#include "engine/assets.hpp"
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
        if (arg == "--stress" || arg == "--no-audio" || arg == "--no-vsync") {
        } else if (arg == "--help")
            result.help = true;
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
    result.validate();
    return result;
}
std::string stress_help() {
    return "Feature Lab stress arena v1: one 60 Hz tick per frame, unpaced.\n"
           "--stress [--cpu-only] --sprites 0..65536 --tiles 0..65536 --entities 0..4096\n"
           "--bodies 0..256 [--dense] --voices 0..16 --seed UINT64\n"
           "--frames 1..10000 --warmup 0..600 --cycles 1..50 (at most 60000 measured frames)\n"
           "--width 64..4096 --height 64..4096 --report FILE.json --label TEXT --screenshot "
           "FILE.ppm\n"
           "Defaults: 8192 sprites, 4096 tiles, 1024 entities, 128 sparse bodies, 8 voices;\n"
           "600 frames after 60 warmup steps per cycle, 3 cycles, 1280x720 offscreen.\n"
           "Mixer runs offline; no audio device. VSync disable is requested. Escape aborts.\n"
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
std::string json_string(std::string_view text) {
    std::string result = "\"";
    constexpr std::string_view hex = "0123456789abcdef";
    for (const char character : text) {
        const auto c = static_cast<unsigned char>(character);
        if (c == '"' || c == '\\') {
            result += '\\';
            result += static_cast<char>(c);
        } else if (c < 32 || c >= 127) {
            // Escape bytes rather than trusting external metadata to be valid UTF-8.
            result += "\\u00";
            result += hex[c >> 4];
            result += hex[c & 15];
        } else
            result += static_cast<char>(c);
    }
    return result + '"';
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
      frame(options.frames * options.cycles), gpu(options.frames * options.cycles) {
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
        << ",\"vsync_requested\":false,\"vsync_disable_accepted\":" << vsync_disable_accepted
        << ",\"audio_device\":false,\"capture_requested\":" << !options.screenshot.empty()
        << "},\n  \"totals\":{\"quads\":" << quads << ",\"culled\":" << culled
        << ",\"draws\":" << draws << ",\"visible_tiles\":" << tiles
        << ",\"collision_candidates\":" << pairs << ",\"contacts\":" << contacts
        << "},\n  \"memory\":{\"tracked_cpu_buffer_bytes\":" << resources.buffer_bytes
        << ",\"report_buffer_bytes\":"
        << load.buffer_bytes() + simulation.buffer_bytes() + audio.buffer_bytes() +
               submission.buffer_bytes() + frame.buffer_bytes() + gpu.buffer_bytes()
        << ",\"rss_start\":" << rss_start << ",\"rss_end\":" << rss_end
        << ",\"rss_peak_at_cycle_boundary\":" << rss_peak
        << ",\"peak_uploaded_textures\":" << texture_peak
        << ",\"peak_uploaded_texture_bytes\":" << texture_bytes_peak
        << ",\"peak_targets\":" << target_peak << ",\"peak_target_bytes\":" << target_bytes_peak
        << "},\n  \"gpu_queries\":{\"supported\":" << gpu_supported
        << ",\"submitted\":" << gpu_submitted << ",\"completed\":" << gpu_completed
        << ",\"pending\":" << gpu_pending << ",\"skipped\":" << gpu_skipped
        << ",\"invalid\":" << gpu_invalid << "},\n  \"milliseconds\":{";
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
    metric("frame", frame, true);
    metric("gpu_render", gpu, true);
    return out.str() + "\n  }\n}\n";
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
