#include "../examples/feature_lab/settings.hpp"
#include "test.hpp"
#include <limits>

namespace {
using namespace engine;
using namespace testing;
constexpr Rect area{{0, 0}, {200, 240}};
auto widgets() {
    return std::array{UiWidget{1, UiKind::button, {{10, 10}, {190, 50}}, "GO"},
                      UiWidget{2, UiKind::checkbox, {{10, 60}, {190, 100}}, "MUTE"},
                      UiWidget{3, UiKind::slider, {{10, 110}, {190, 150}}, "GAIN", 0.5F},
                      UiWidget{4, UiKind::button, {{10, 160}, {190, 200}}, "DISABLED", 0, false}};
}
InputFrame key(Key k) {
    InputFrame result{};
    result[static_cast<std::size_t>(k)] = {true, true, false};
    return result;
}
TEST(pointer_edges_and_focus_cancel) {
    Input input;
    input.move_pointer({12, 14});
    input.set_primary(true);
    input.move_pointer({30, 40});
    input.set_primary(false);
    input.scroll(2);
    auto f = input.consume();
    CHECK(f.pointer.primary.pressed && f.pointer.primary.released && !f.pointer.primary.held);
    NEAR(f.pointer.press_position.x, 12.0F);
    NEAR(f.pointer.release_position.x, 30.0F);
    CHECK(f.pointer.wheel == 2);
    f = input.consume();
    CHECK(!f.pointer.primary.pressed && !f.pointer.primary.released && f.pointer.wheel == 0);
    input.scroll(std::numeric_limits<int>::max());
    input.scroll(std::numeric_limits<int>::max());
    CHECK(input.consume().pointer.wheel == 120);
    input.set_primary(true);
    input.release_all();
    f = input.consume();
    CHECK(f.canceled && !f.pointer.primary.held && !f.pointer.primary.released);
    CHECK(!input.consume().canceled);
    rejects([&] { input.move_pointer({NAN, 0}); });
}
TEST(layout_validation_preserves_live_state) {
    Ui ui;
    auto ws = widgets();
    ui.layout(area, ws);
    ws[1].id = 1;
    rejects([&] { ui.layout(area, ws); });
    CHECK(ui.widgets().size() == 4 && ui.widgets()[1].id == 2 && ui.focused() == 1);
    ws = widgets();
    ws[2].value = NAN;
    rejects([&] { ui.layout(area, ws); });
    ws = widgets();
    ws[0].bounds.max = ws[0].bounds.min;
    rejects([&] { ui.layout(area, ws); });
    rejects([&] { ui.set_value(2, 0.3F); });
    rejects([&] { ui.set_value(999, 0); });
    std::array<UiWidget, 65> too_many{};
    rejects([&] { ui.layout(area, too_many); });
    rejects([&] { ui.layout({}, widgets()); });
}
TEST(keyboard_focus_skips_disabled_and_wraps) {
    Ui ui;
    ui.layout(area, widgets());
    CHECK(ui.focused() == 1);
    ui.update(key(Key::tab));
    CHECK(ui.focused() == 2);
    auto action = ui.update(key(Key::space));
    CHECK(action.count == 1 && action.actions[0].id == 2 && action.actions[0].value == 1);
    ui.update(key(Key::tab));
    CHECK(ui.focused() == 3);
    ui.update(key(Key::tab));
    CHECK(ui.focused() == 1);
    auto back = key(Key::tab);
    back[static_cast<std::size_t>(Key::shift)].held = true;
    ui.update(back);
    CHECK(ui.focused() == 3);
    ui.update(key(Key::up));
    CHECK(ui.focused() == 2);
    auto held = key(Key::accept);
    held[static_cast<std::size_t>(Key::accept)].pressed = false;
    CHECK(ui.update(held).count == 0);
}
TEST(pointer_activation_requires_matching_press_and_release) {
    Ui ui;
    ui.layout(area, widgets());
    Input input;
    input.move_pointer({20, 20});
    input.set_primary(true);
    CHECK(ui.update(input.consume()).count == 0 && ui.captured() == 1);
    input.move_pointer({210, 20}, false);
    input.set_primary(false);
    CHECK(ui.update(input.consume()).count == 0 && ui.captured() == 0);
    input.set_primary(true); // Outside press, inside release does not activate.
    input.move_pointer({20, 20});
    input.set_primary(false);
    CHECK(ui.update(input.consume()).count == 0);
    input.set_primary(true); // Fast tap inside in one tick is retained.
    input.set_primary(false);
    const auto result = ui.update(input.consume(), false);
    CHECK(result.count == 1 && result.actions[0].id == 1 && result.pointer);
    CHECK(ui.update(input.consume()).count == 0);
    input.set_primary(true);
    ui.update(input.consume());
    input.release_all();
    CHECK(ui.update(input.consume()).count == 0 && ui.captured() == 0);
}
TEST(slider_capture_clamp_wheel_and_keyboard) {
    Ui ui;
    ui.layout(area, widgets());
    Input input;
    input.move_pointer({100, 130});
    input.set_primary(true);
    ui.update(input.consume());
    input.move_pointer({400, -50}, false);
    auto changed = ui.update(input.consume());
    CHECK(changed.count == 1 && changed.actions[0].value == 1 && ui.captured() == 3);
    input.move_pointer({-20, -50}, false);
    input.set_primary(false);
    changed = ui.update(input.consume());
    CHECK(changed.count == 1 && changed.actions[0].value == 0 && ui.captured() == 0);
    changed = ui.update(key(Key::right));
    NEAR(changed.actions[0].value, 0.05F);
    input.move_pointer({100, 130});
    input.scroll(3);
    changed = ui.update(input.consume());
    NEAR(changed.actions[0].value, 0.2F);
    CHECK(ui.update(input.consume()).count == 0);
}
TEST(clipping_overlap_and_resize_cancel_capture) {
    Ui ui;
    auto ws = widgets();
    ws[3].bounds = ws[0].bounds; // Disabled top widget blocks click-through.
    ui.layout({{0, 0}, {100, 200}}, ws);
    Input input;
    const auto click = [&](Vec2 p) {
        input.move_pointer(p);
        input.set_primary(true);
        input.set_primary(false);
        return ui.update(input.consume()).count;
    };
    CHECK(click({20, 20}) == 0);
    CHECK(click({150, 80}) == 0); // Outside clip, inside checkbox.
    CHECK(click({50, 80}) == 1);
    input.move_pointer({50, 130});
    input.set_primary(true);
    ui.update(input.consume());
    CHECK(ui.captured() == 3);
    ui.layout(area, ws);
    input.set_primary(false);
    CHECK(ui.update(input.consume()).count == 0 && ui.captured() == 0);
    for (auto& w : ws)
        w.enabled = false;
    ui.layout(area, ws);
    CHECK(ui.focused() == 0 && ui.update(key(Key::accept)).count == 0);
}
TEST(input_gate_holds_capture_until_release) {
    Input input;
    InputGate gate;
    input.set(Key::right, true);
    input.set_primary(true);
    CHECK(!button(gate.route(input.consume(), true, true), Key::right).held);
    auto f = gate.route(input.consume(), false, false);
    CHECK(!button(f, Key::right).held && !f.pointer.primary.held);
    input.set(Key::right, false);
    input.set_primary(false);
    gate.route(input.consume(), false, false);
    input.set(Key::right, true);
    input.set_primary(true);
    f = gate.route(input.consume(), false, false);
    CHECK(button(f, Key::right).pressed && f.pointer.primary.pressed);
}
TEST(column_rejects_overflow_without_advancing) {
    UiColumn c({{0, 0}, {100, 100}}, 10);
    NEAR(c.next(40).max.y, 40.0F);
    rejects([&] { c.next(60); });
    NEAR(c.next(50).min.y, 50.0F);
    rejects([&] { UiColumn invalid(area, -1); });
}
TEST(settings_pause_restore_and_gameplay_isolation) {
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Settings settings;
    Input input;
    input.set(Key::settings, true);
    auto result = settings.update(game, input.consume(), 1280, 720, true);
    CHECK(result.captured && settings.opened() && game.paused);
    const auto ticks = game.ticks;
    input.set(Key::settings, false);
    input.set(Key::right, true);
    result = settings.update(game, input.consume(), 1280, 720, true);
    game.update(result.gameplay);
    CHECK(game.ticks == ticks && !button(result.gameplay, Key::right).held);
    input.set(Key::escape, true);
    result = settings.update(game, input.consume(), 1280, 720, true);
    CHECK(!settings.opened() && !game.paused && result.captured);
    CHECK(!button(result.gameplay, Key::escape).pressed);
    result = settings.update(game, input.consume(), 1280, 720, true);
    CHECK(!button(result.gameplay, Key::right).held && !result.captured);
    game.paused = true;
    settings.update(game, key(Key::settings), 640, 480, false);
    CHECK(settings.opened());
    settings.update(game, key(Key::accept), 640, 480, false); // Resume retains prior pause.
    CHECK(!settings.opened() && game.paused);
}
TEST(settings_actions_and_responsive_bounds) {
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Settings settings;
    settings.update(game, key(Key::settings), 1280, 720, true);
    settings.update(game, key(Key::tab), 1280, 720, true); // Master.
    auto result = settings.update(game, key(Key::left), 1280, 720, true);
    CHECK(result.audio_changed);
    NEAR(settings.audio.master, 0.95F);
    for (const auto size : {Vec2{320, 240}, Vec2{640, 480}, Vec2{1920, 1080}}) {
        settings.update(game, {}, static_cast<int>(size.x), static_cast<int>(size.y), false);
        const auto clip = settings.ui().clip();
        CHECK(clip.min.x >= 0 && clip.min.y >= 0 && clip.max.x <= size.x && clip.max.y <= size.y);
        for (const auto& w : settings.ui().widgets())
            CHECK(w.bounds.min.y >= clip.min.y && w.bounds.max.y <= clip.max.y);
    }
    settings.reject_vsync(true);
    CHECK(!settings.vsync_available && settings.vsync);
    // Click restart after resize; action injects exactly one restart and closes modal.
    Input input;
    for (const auto& w : settings.ui().widgets())
        if (w.id == feature_lab::Settings::restart)
            input.move_pointer((w.bounds.min + w.bounds.max) * 0.5F);
    input.set_primary(true);
    input.set_primary(false);
    result = settings.update(game, input.consume(), 1920, 1080, false);
    CHECK(result.restart && !settings.opened());
    CHECK(button(result.gameplay, Key::restart).pressed);
    game.update(result.gameplay);
    CHECK(!game.paused && game.count() == 0);
}
TEST(settings_buttons_checkboxes_and_disabled_audio) {
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Settings settings;
    settings.update(game, key(Key::settings), 1280, 720, true);
    const auto click = [&](engine::UiId id, bool audio_available = true) {
        Input input;
        for (const auto& w : settings.ui().widgets())
            if (w.id == id)
                input.move_pointer((w.bounds.min + w.bounds.max) * 0.5F);
        input.set_primary(true);
        input.set_primary(false);
        return settings.update(game, input.consume(), 1280, 720, audio_available);
    };
    CHECK(click(feature_lab::Settings::mute).audio_changed && settings.audio.muted);
    CHECK(click(feature_lab::Settings::music).audio_changed);
    NEAR(settings.audio.music, 0.5F);
    CHECK(click(feature_lab::Settings::effects).audio_changed == false); // Already 50%.
    CHECK(click(feature_lab::Settings::sync).vsync_changed && !settings.vsync);
    CHECK(click(feature_lab::Settings::fullscreen).fullscreen);
    CHECK(click(feature_lab::Settings::quit).quit);
    settings.update(game, {}, 1280, 720, false);
    CHECK(!click(feature_lab::Settings::mute, false).audio_changed);
    CHECK(!click(feature_lab::Settings::music, false).audio_changed);
    const auto result = settings.update(game, key(Key::fullscreen), 1280, 720, false);
    CHECK(!button(result.gameplay, Key::fullscreen).pressed);
    CHECK(click(feature_lab::Settings::resume, false).captured && !settings.opened());
}

TEST(settings_save_load_and_slot_selection) {
    auto game = feature_lab::load_game(AssetRoot(TEST_ASSET_ROOT));
    feature_lab::Settings settings;
    settings.update(game, key(Key::settings), 1280, 720, false);
    const auto click = [&](UiId id) {
        Input input;
        for (const auto& widget : settings.ui().widgets())
            if (widget.id == id)
                input.move_pointer((widget.bounds.min + widget.bounds.max) * 0.5F);
        input.set_primary(true);
        input.set_primary(false);
        return settings.update(game, input.consume(), 1280, 720, false);
    };
    CHECK(settings.selected_slot == 1);
    CHECK(click(feature_lab::Settings::save).save && settings.opened());
    click(feature_lab::Settings::slot);
    CHECK(settings.selected_slot == 2);
    CHECK(click(feature_lab::Settings::load).load && game.paused);
    click(feature_lab::Settings::slot);
    CHECK(settings.selected_slot == 3);
    click(feature_lab::Settings::slot);
    CHECK(settings.selected_slot == 1);
}

} // namespace
int main() {
    return testing::run_tests();
}
