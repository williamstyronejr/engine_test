#include "../examples/feature_lab/game_draw.hpp"
#include "../examples/feature_lab/replay_file.hpp"
#include "../examples/feature_lab/settings_draw.hpp"
#include "../examples/feature_lab/stress_benchmark.hpp"
#include "allocation_profile.hpp"
#include "engine/window.hpp"
#include "stress_build.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
using namespace engine;
using namespace feature_lab;
constexpr int width = 1280, height = 800;
constexpr unsigned frames = 300, warmup = 30;
enum Scope : std::size_t { update, minimap, world, hud, diagnostics, settings, end, frame, count };
constexpr std::array scope_names{
    "settings_update",    "minimap_submit",  "world_submit", "hud_submit",
    "diagnostics_submit", "settings_submit", "render_end",   "frame"};
struct Scenario {
    const char* name;
    bool audio{}, remap{}, menu{}, controls{}, capture{}, rebind{}, stats{}, shapes{};
    unsigned gpu_mode{};
};
#ifdef DIAGNOSTICS_MATRIX
constexpr std::array scenarios{
    Scenario{"gpu_unsupported", true, false, false, false, false, false, true},
    Scenario{"gpu_waiting", true, false, false, false, false, false, true, false, 1},
    Scenario{"gpu_completed", true, false, false, false, false, false, true, false, 2},
    Scenario{"gpu_changing", true, false, false, false, false, false, true, true, 3},
    Scenario{"combined_settings", true, false, true, false, false, false, true, true, 3},
    Scenario{"combined_controls", true, true, true, true, false, false, true, true, 3}};
constexpr const char* usage = "Usage: diagnostics_allocations REPORT.json [SCREENSHOT_DIRECTORY]";
constexpr const char* workload = "diagnostics_ui_states_v1";
constexpr const char* gpu_history = "synthetic";
#else
constexpr std::array scenarios{
    Scenario{"audio_off"},
    Scenario{"audio_default", true},
    Scenario{"audio_remapped", true, true},
    Scenario{"settings_default", true, false, true},
    Scenario{"controls_remapped", true, true, true, true},
    Scenario{"binding_capture", true, true, true, true, true},
    Scenario{"binding_updated", true, true, true, true, true, true},
    Scenario{"diagnostics", true, false, false, false, false, false, true},
    Scenario{"diagnostics_shapes", true, false, false, false, false, false, true, true}};
constexpr const char* usage = "Usage: ui_allocations REPORT.json [SCREENSHOT_DIRECTORY]";
constexpr const char* workload = "frozen_ui_states_v1";
constexpr const char* gpu_history = "none";
#endif
using Summary = allocation_profile::Summary;
using Count = allocation_profile::Count;
void accumulate(Summary& out, Count before, Count after) {
    const auto calls = after.calls - before.calls, bytes = after.bytes - before.bytes;
    ++out.samples;
    out.nonzero_samples += calls != 0;
    out.calls += calls;
    out.bytes += bytes;
    out.max_calls = std::max(out.max_calls, calls);
    out.max_bytes = std::max(out.max_bytes, bytes);
}
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
struct Result {
    std::array<Summary, count> scopes{};
    // Opening, entering controls, starting capture and accepting a rebind are
    // deliberate one-off layout/string work, separate from idle redraws.
    std::array<Summary, 4> transitions{};
    RenderStats window{}, map{};
    std::uint64_t hash{}, ticks{};
    bool audio{}, menu{}, controls{}, capture{}, stats{}, shapes{};
    std::string audio_label, binding_label;
    std::array<unsigned, 3> gpu_phases{};
    std::uint64_t render_fingerprint{1469598103934665603ULL};
};
void click(Settings& menu, Game& game, UiId id, bool audio) {
    const auto widgets = menu.ui().widgets();
    const auto found = std::find_if(widgets.begin(), widgets.end(),
                                    [&](const UiWidget& widget) { return widget.id == id; });
    require(found != widgets.end(), "Missing menu control");
    InputFrame input{};
    input.pointer.position = (found->bounds.min + found->bounds.max) * 0.5F;
    input.pointer.press_position = input.pointer.release_position = input.pointer.position;
    input.pointer.inside = true;
    input.pointer.primary = {true, true, false};
    menu.update(game, input, width, height, audio);
    input.pointer.primary = {false, false, true};
    menu.update(game, input, width, height, audio);
}
#ifdef DIAGNOSTICS_MATRIX
// Rebuild synthetic history outside measured rendering. Values intentionally cross
// text-width/overflow boundaries; no GL timing or real-time performance is claimed.
unsigned prepare_diagnostics(Diagnostics& debug, Scenario scenario, unsigned iteration) {
    debug = {};
    debug.controls = {scenario.stats, scenario.shapes};
    const unsigned phase = scenario.gpu_mode == 3 ? iteration % 30 / 10 : scenario.gpu_mode;
    const unsigned variation = scenario.gpu_mode == 3 ? iteration % 4 : 2;
    constexpr std::array values{0.0, 12.125, 9999.5, std::numeric_limits<double>::max()};
    const double value = values[variation];
    for (unsigned i = 0; i < 240; ++i)
        debug.record({value, value}, {1234 + iteration, 12, 8}, {456, 0, 3}, iteration % 10);
    GpuTimingStats stats;
    stats.supported = phase != 0;
    stats.counter_bits = stats.supported ? 64 : 0;
    stats.pending = phase == 1 ? 8 : iteration % 9;
    stats.submitted = 1000 + iteration;
    stats.skipped = iteration * 17;
    stats.invalid = iteration / 17;
    const auto tag = std::uint64_t{4} * iteration + 2;
    const std::array samples{GpuTimingSample{tag - 1, value}, GpuTimingSample{tag, value}};
    if (phase == 2) {
        stats.completed = 2;
        debug.record_gpu(samples, stats, tag + iteration % 15);
    } else {
        debug.record_gpu({}, stats, tag);
    }
    require(debug.gpu_summary().samples == (phase == 2 ? 2U : 0U), "Wrong synthetic GPU history");
    return phase;
}
#endif
Result run(const AssetRoot& assets, Level level, Scenario scenario,
           const std::filesystem::path& screenshots, GraphicsInfo& graphics) {
    Renderer renderer;
    graphics = renderer.graphics_info();
    const auto target = renderer.create_target(width, height);
    auto game = load_game(assets, level);
    Replay replay;
    for (int i = 0; i < 90; ++i)
        game.update(replay.next(game));
    FontAssets fonts;
    require(fonts.reload(renderer, assets), "Font fixture failed");
    ShaderTools shaders;
    shaders.reload(renderer, assets);
    require(shaders.result().applied, "Shader fixture failed");
    TextureAssets textures(assets);
    textures.load(renderer, game.map.data().texture, game.map.data().atlas_columns,
                  game.map.data().atlas_rows);
    textures.load(renderer, game.animations->texture, game.animations->atlas_columns,
                  game.animations->atlas_rows);
    game.world.each([&](const SceneNode& node) {
        if (node.sprite)
            textures.load(renderer, node.sprite->texture, 1, 1);
    });
    Overlays overlays(renderer, game);
    CollisionVisuals collision_visuals(renderer);
    SamplingGallery sampling_gallery(renderer);
    AnimationAssets animations;
    TilemapAssets tilemaps;
    SceneAssets scenes;
    Diagnostics debug;
    debug.controls = {scenario.stats, scenario.shapes};
    // Fixed CPU history makes label widths and the fixture reproducible. No GPU
    // timing queries are issued: the unsupported/default branch is explicit.
    for (unsigned i = 0; i < 240; ++i)
        debug.record({16.75, 2.25}, {1234, 12, 8}, {456, 0, 3}, 1);
    Settings menu;
    if (scenario.remap) {
        menu.bindings.assign(0, 0xe9, modifiers::all);
        menu.bindings.assign(6, mouse_symbol(MouseButton::side2), modifiers::all);
        menu.bindings.assign(7, 0xf1, modifiers::all);
        menu.bindings.assign(8, 0x010003bb, modifiers::control | modifiers::shift);
        menu.bindings.assign(9, 0x0101f642, modifiers::all);
        menu.audio = {0.75F, 0.15F, 0.25F, true};
    }
    Soundtrack track(assets, scenario.audio, "null", menu.audio);
    require(track.enabled() == scenario.audio, "Requested ALSA null audio unavailable");
    track.locate_emitter(game);
    Result result;
    const auto transition = [&](std::size_t index, auto action) {
        const auto before = allocation_profile::snapshot();
        action();
        accumulate(result.transitions[index], before, allocation_profile::snapshot());
    };
    if (scenario.menu) {
        transition(0, [&] {
            InputFrame input{};
            input[static_cast<std::size_t>(Key::settings)].pressed = true;
            menu.update(game, input, width, height, scenario.audio);
        });
    }
    if (scenario.controls)
        transition(1, [&] { click(menu, game, Settings::controls, scenario.audio); });
    if (scenario.capture)
        transition(2, [&] { click(menu, game, Settings::binding_first, scenario.audio); });
    if (scenario.rebind) {
        transition(3, [&] {
            InputFrame input{};
            input.pressed_symbol = 0xf6;
            input.pressed_modifiers = modifiers::control | modifiers::shift;
            const auto step = menu.update(game, input, width, height, scenario.audio);
            require(step.bindings_changed, "Rebind was not accepted");
        });
    }
    require(menu.opened() == scenario.menu && menu.controls_opened() == scenario.controls &&
                menu.capturing_binding() == (scenario.capture && !scenario.rebind),
            "Menu fixture did not reach requested state");
    track.update(game, {}, 0);
    if (scenario.audio) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (track.stats().started != 2 || track.stats().written_frames == 0) {
            require(std::chrono::steady_clock::now() < deadline, "ALSA null startup timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    result.hash = replay_hash(game);
    result.ticks = game.ticks;
    result.audio = track.enabled();
    result.menu = menu.opened();
    result.controls = menu.controls_opened();
    result.capture = menu.capturing_binding();
    result.stats = debug.controls.visible;
    result.shapes = debug.controls.shapes;
    result.audio_label = track.status(menu.bindings);
    result.binding_label = menu.bindings.label(0);
    std::string text, status;
    text.reserve(256);
    status.reserve(128);
    format_text(status, "36 / ", level_name(level), " / AUDIO BANK RELOAD");
    const auto camera = game.camera(width, height, 1);
    for (unsigned i = 0; i < frames; ++i) {
#ifdef DIAGNOSTICS_MATRIX
        const auto phase = prepare_diagnostics(debug, scenario, scenario.gpu_mode == 3 ? i : 0);
        ++result.gpu_phases[phase];
#endif
        const auto start = allocation_profile::snapshot();
        menu.update(game, {}, width, height, scenario.audio);
        const auto updated = allocation_profile::snapshot();
        overlays.render_minimap(renderer, game, width, height, 1);
        const auto mapped = allocation_profile::snapshot();
        renderer.begin(target, camera);
        Count world_done{};
        draw_game(renderer, game, textures, 1, width, height, camera, overlays, track,
                  collision_visuals, sampling_gallery, debug, shaders, fonts, animations, tilemaps,
                  scenes, menu.opened(), menu.bindings, status, false, false, text, nullptr,
                  &world_done);
        const auto hud_done = allocation_profile::snapshot();
        debug.draw_hud(renderer, game, width, height, 0.125, text);
        const auto debug_done = allocation_profile::snapshot();
        menu.draw(renderer, width, height);
        const auto menu_done = allocation_profile::snapshot();
        renderer.end();
        const auto finished = allocation_profile::snapshot();
        if (i >= warmup) {
            const std::array points{start,    updated,    mapped,    world_done,
                                    hud_done, debug_done, menu_done, finished};
            for (std::size_t scope = 0; scope < frame; ++scope)
                accumulate(result.scopes[scope], points[scope], points[scope + 1]);
            accumulate(result.scopes[frame], start, finished);
        }
        const auto drawn = renderer.stats();
        require(drawn.quads > 100 && drawn.draws > 0 && renderer.healthy(), "Empty/unhealthy draw");
        if (i && scenario.gpu_mode != 3) {
            require(result.window.quads == drawn.quads && result.window.draws == drawn.draws &&
                        result.window.culled == drawn.culled,
                    "Frozen fixture render totals changed");
        }
        result.window = drawn;
        result.map = overlays.minimap_stats();
        for (const auto value : {drawn.quads, drawn.draws, drawn.culled}) {
            result.render_fingerprint ^= value;
            result.render_fingerprint *= 1099511628211ULL;
        }
#ifdef DIAGNOSTICS_MATRIX
        if (!screenshots.empty() && scenario.gpu_mode == 3 && (i == 0 || i == 10 || i == 20)) {
            const auto name = std::string(level_name(level)) + "-" + scenario.name + "-" +
                              std::to_string(i) + ".ppm";
            renderer.screenshot((screenshots / name).string());
        }
#endif
    }
    require(replay_hash(game) == result.hash && game.ticks == result.ticks,
            "Idle UI workload changed gameplay");
    require(track.enabled() == scenario.audio, "Audio unexpectedly disabled");
    if (scenario.audio)
        require(!track.stats().queue_rejections && !track.stats().dropped_plays &&
                    !track.stats().stale_controls,
                "Audio fixture rejected commands");
    if (!screenshots.empty()) {
        const auto name = std::string(level_name(level)) + "-" + scenario.name + ".ppm";
        renderer.screenshot((screenshots / name).string());
    }
    return result; // Soundtrack joins its worker before any report serialization.
}
void summary(std::ostream& out, const Summary& value) {
    out << "{\"samples\":" << value.samples << ",\"nonzero_samples\":" << value.nonzero_samples
        << ",\"calls\":" << value.calls << ",\"requested_bytes\":" << value.bytes
        << ",\"max_calls\":" << value.max_calls << ",\"max_requested_bytes\":" << value.max_bytes
        << '}';
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 2 && argc != 3)
            throw std::invalid_argument(usage);
        if (!std::getenv("DISPLAY") || !*std::getenv("DISPLAY")) {
            std::cout << "SKIP: UI allocation workload DISPLAY unavailable\n";
            return 77;
        }
        const std::filesystem::path screenshots = argc == 3 ? argv[2] : "";
        if (!screenshots.empty())
            std::filesystem::create_directories(screenshots);
        const AssetRoot assets(TEST_ASSET_ROOT);
        Window window(64, 64, "UI allocations", false);
        GraphicsInfo graphics;
        std::array<Result, scenarios.size() * 2> results;
        std::size_t index = 0;
        for (const auto level : {Level::facility, Level::annex})
            for (const auto scenario : scenarios)
                results[index++] = run(assets, level, scenario, screenshots, graphics);
        std::ofstream out(argv[1]);
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out << "{\"schema\":\"feature_lab_ui_allocations_v1\",\"revision\":"
            << json_string(stress_build_revision)
            << ",\"compiler\":" << json_string(stress_build_compiler)
            << ",\"configuration\":" << json_string(stress_build_configuration)
            << ",\"flags\":" << json_string(stress_build_flags)
            << ",\"workload\":" << json_string(workload)
            << ",\"complete\":true,\"frames\":" << frames << ",\"warmup\":" << warmup
            << ",\"measured_frames\":" << frames - warmup << ",\"width\":" << width
            << ",\"height\":" << height
            << ",\"audio_device\":\"null\",\"thread\":\"render\",\"gpu_history\":"
            << json_string(gpu_history)
            << ",\"graphics\":{\"vendor\":" << json_string(graphics.vendor)
            << ",\"device\":" << json_string(graphics.device)
            << ",\"version\":" << json_string(graphics.version) << "},\"scenarios\":[";
        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto& r = results[i];
            out << (i ? "," : "")
                << "{\"level\":" << json_string(i < scenarios.size() ? "facility" : "annex")
                << ",\"name\":" << json_string(scenarios[i % scenarios.size()].name)
                << ",\"state\":{\"audio\":" << (r.audio ? "true" : "false")
                << ",\"settings\":" << (r.menu ? "true" : "false")
                << ",\"controls\":" << (r.controls ? "true" : "false")
                << ",\"capture\":" << (r.capture ? "true" : "false")
                << ",\"diagnostics\":" << (r.stats ? "true" : "false")
                << ",\"shapes\":" << (r.shapes ? "true" : "false")
                << ",\"audio_label\":" << json_string(r.audio_label)
                << ",\"binding_label\":" << json_string(r.binding_label) << "},\"hash\":" << r.hash
                << ",\"ticks\":" << r.ticks << ",\"render\":{\"quads\":" << r.window.quads
                << ",\"draws\":" << r.window.draws << ",\"culled\":" << r.window.culled
                << ",\"minimap_quads\":" << r.map.quads << ",\"minimap_draws\":" << r.map.draws
                << "},\"scopes\":{";
            for (std::size_t j = 0; j < count; ++j) {
                out << (j ? "," : "") << json_string(scope_names[j]) << ':';
                summary(out, r.scopes[j]);
            }
#ifdef DIAGNOSTICS_MATRIX
            out << "},\"gpu_phases\":[" << r.gpu_phases[0] << ',' << r.gpu_phases[1] << ','
                << r.gpu_phases[2] << "],\"render_fingerprint\":" << r.render_fingerprint;
            out << ",\"transitions\":{";
#else
            out << "},\"transitions\":{";
#endif
            constexpr std::array names{"open", "controls", "capture", "rebind"};
            for (std::size_t j = 0; j < names.size(); ++j) {
                out << (j ? "," : "") << json_string(names[j]) << ':';
                summary(out, r.transitions[j]);
            }
            out << "}}";
        }
        out << "]}\n";
        out.close();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
