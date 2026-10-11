#include "../examples/feature_lab/persistence.hpp"
#include "test.hpp"

namespace {
using namespace testing;
using namespace engine;
using namespace feature_lab;
TEST(binding_assignment_is_validated_and_transactional) {
    KeyBindings bindings;
    CHECK(bindings.action('w') == Key::up && !bindings.action('q'));
    const auto before = bindings;
    rejects([&] { bindings.assign(0, 'w'); });
    rejects([&] { bindings.assign(0, 0); });
    rejects([&] { bindings.assign(0, 0xff1b); });
    rejects([&] { bindings.assign(10, 'q'); });
    CHECK(bindings == before);
    bindings.assign(0, 'q');
    CHECK(bindings.action('q') == Key::left && !bindings.action('a'));
    CHECK(bindings.label(0) == "MOVE LEFT : Q" && bindings.key_name(Key::left) == "Q");
    rejects([&] { bindings.key_name(Key::settings); });
    bindings.assign(0, 'q'); // Assigning the current letter is valid.
    bindings.validate();
}
TEST(mouse_bindings_are_unique_and_have_readable_names) {
    KeyBindings bindings;
    const std::array buttons{MouseButton::left, MouseButton::middle, MouseButton::right,
                             MouseButton::side1, MouseButton::side2};
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        bindings.assign(i, mouse_symbol(buttons[i]));
        CHECK(bindings.action(mouse_symbol(buttons[i])) == bindable_actions[i]);
    }
    CHECK(bindings.label(0) == "MOVE LEFT : LMB");
    CHECK(bindings.key_name(Key::up) == "RMB");
    const auto before = bindings;
    rejects([&] { bindings.assign(6, mouse_symbol(MouseButton::right)); });
    for (const auto invalid : {0x20000000U, 0x20000006U, 0xffffffffU, 4U, 5U})
        rejects([&] { bindings.assign(6, invalid); });
    CHECK(bindings == before);
    bindings.assign(0, 'a');
    CHECK(!bindings.action(mouse_symbol(MouseButton::left)));
}
TEST(raw_press_queue_cancellation_and_modal_capture) {
    Input input;
    input.press_symbol('q');
    input.press_symbol('z');
    CHECK(input.consume().pressed_symbol == 'q');
    CHECK(input.consume().pressed_symbol == 0);
    input.press_symbol('q');
    input.release_all();
    CHECK(input.consume().pressed_symbol == 0);
    InputGate gate;
    InputFrame frame{};
    frame.pressed_symbol = 'q';
    CHECK(gate.route(frame, true, false).pressed_symbol == 0);
    CHECK(gate.route(frame, false, false).pressed_symbol == 'q');
    frame.pressed_symbol = mouse_symbol(MouseButton::right);
    CHECK(gate.route(frame, false, true).pressed_symbol == 0);
    CHECK(gate.route(frame, true, false).pressed_symbol == 0);
    CHECK(gate.route(frame, false, false).pressed_symbol == mouse_symbol(MouseButton::right));
    input.press_symbol(mouse_symbol(MouseButton::left));
    input.press_symbol('q');
    CHECK(input.consume().pressed_symbol == mouse_symbol(MouseButton::left));
    CHECK(!input.consume().pressed_symbol);
}
TEST(configuration_v3_roundtrip_and_legacy_migration) {
    Configuration config;
    config.bindings.assign(0, 'q');
    auto bytes = encode_config(config);
    CHECK(bytes.size() == 72 && decode_config(bytes) == config);
    binary::Writer legacy("ECFG");
    legacy.f32(0.5F);
    legacy.f32(0.25F);
    legacy.f32(0.75F);
    legacy.u32(1);
    legacy.u32(0);
    const auto restored = decode_config(seal_record(legacy.take()));
    CHECK(restored.audio.master == 0.5F && restored.audio.muted && !restored.vsync);
    CHECK(restored.bindings == KeyBindings{});
    CHECK(decode_config(encode_config(restored)) == restored);
    auto legacy_v2 = std::vector<std::uint8_t>(bytes.begin(), bytes.end() - 4);
    legacy_v2[4] = 2;
    CHECK(decode_config(seal_record(legacy_v2)) == config);
    config.bindings.assign(6, mouse_symbol(MouseButton::right));
    CHECK(decode_config(encode_config(config)) == config);
    const auto mouse_bytes = encode_config(config);
    auto mouse_payload = open_record(mouse_bytes);
    auto invalid = std::vector<std::uint8_t>(mouse_payload.begin(), mouse_payload.end());
    invalid[4] = 2; // v2 remains strictly letters, even with a valid checksum.
    rejects([&] { decode_config(seal_record(invalid)); });
    invalid[4] = 3;
    invalid[28 + 6 * 4] = 6; // Unknown mouse symbol.
    rejects([&] { decode_config(seal_record(invalid)); });
    invalid[28 + 6 * 4] = 3;
    std::copy_n(invalid.begin() + 28 + 6 * 4, 4, invalid.begin() + 28);
    rejects([&] { decode_config(seal_record(invalid)); });
    bytes[28] = 'd'; // Duplicate right binding with a valid checksum.
    auto payload = std::vector<std::uint8_t>(bytes.begin(), bytes.end() - 4);
    rejects([&] { decode_config(seal_record(payload)); });
    payload[28] = 0;
    rejects([&] { decode_config(seal_record(payload)); });
    payload[28] = 'q';
    payload[5] = 1; // High version bytes must also be checked.
    rejects([&] { decode_config(seal_record(payload)); });
}
struct Menu {
    Game game = load_game(AssetRoot(TEST_ASSET_ROOT));
    Settings settings;
    int width{640}, height{480};
    Menu() {
        InputFrame input{};
        input[static_cast<std::size_t>(Key::settings)].pressed = true;
        update(input);
        click(Settings::controls);
    }
    SettingsStep update(InputFrame input = {}) {
        return settings.update(game, input, width, height, false);
    }
    SettingsStep click(UiId id) {
        Rect bounds{};
        bool found = false;
        for (const auto& widget : settings.ui().widgets())
            if (widget.id == id) {
                bounds = widget.bounds;
                found = true;
            }
        CHECK(found);
        Input pointer;
        pointer.move_pointer((bounds.min + bounds.max) * 0.5F);
        pointer.set_primary(true);
        update(pointer.consume());
        pointer.set_primary(false);
        return update(pointer.consume());
    }
};
TEST(settings_rebind_conflicts_capture_defaults_and_close) {
    Menu m;
    CHECK(m.settings.controls_opened() && m.game.paused);
    m.click(Settings::binding_first);
    CHECK(m.settings.capturing_binding());
    InputFrame frame{};
    frame.pressed_symbol = 'w';
    CHECK(!m.update(frame).bindings_changed);
    CHECK(m.settings.bindings == KeyBindings{} && m.settings.capturing_binding());
    CHECK(m.settings.notice == "INPUT ALREADY ASSIGNED");
    frame.pressed_symbol = 0xff0d; // Fixed Enter cannot be reassigned.
    CHECK(!m.update(frame).bindings_changed && m.settings.capturing_binding());
    frame.pressed_symbol = 'q';
    frame[static_cast<std::size_t>(Key::left)] = {true, true, false};
    auto result = m.update(frame);
    CHECK(result.bindings_changed && !m.settings.capturing_binding());
    CHECK(result.gameplay.pressed_symbol == 0 && !button(result.gameplay, Key::left).pressed);
    CHECK(m.settings.bindings.action('q') == Key::left);
    CHECK(m.click(Settings::defaults).bindings_changed);
    CHECK(m.settings.bindings == KeyBindings{});
    m.click(Settings::controls_back);
    CHECK(!m.settings.controls_opened() && m.settings.opened());
    m.click(Settings::resume);
    CHECK(!m.settings.opened() && !m.game.paused);
}
TEST(mouse_capture_conflicts_and_held_actions_do_not_escape_the_menu) {
    Menu m;
    m.click(Settings::binding_first + 6);
    CHECK(m.settings.capturing_binding()); // Opening click is never the binding.
    InputFrame frame{};
    frame.pressed_symbol = mouse_symbol(MouseButton::right);
    CHECK(m.update(frame).bindings_changed);
    CHECK(m.settings.bindings.key_name(Key::interact) == "RMB");
    m.click(Settings::binding_first);
    CHECK(!m.update(frame).bindings_changed && m.settings.capturing_binding());
    frame.pressed_symbol = mouse_symbol(MouseButton::left);
    frame.pointer.primary = {true, true, false};
    frame[static_cast<std::size_t>(Key::left)] = {true, true, false};
    CHECK(m.update(frame).bindings_changed);
    frame.pressed_symbol = 0;
    frame.pointer.primary.pressed = false;
    frame[static_cast<std::size_t>(Key::left)].pressed = false;
    frame[static_cast<std::size_t>(Key::settings)].pressed = true;
    CHECK(!button(m.update(frame).gameplay, Key::left).held);
    CHECK(!m.settings.opened());
    frame[static_cast<std::size_t>(Key::settings)] = {};
    CHECK(!button(m.update(frame).gameplay, Key::left).held);
    CHECK(!m.update(frame).gameplay.pointer.primary.held);
    frame.pointer.primary = {false, false, true};
    frame[static_cast<std::size_t>(Key::left)] = {false, false, true};
    m.update(frame);
    frame.pointer.primary = {true, true, false};
    frame[static_cast<std::size_t>(Key::left)] = {true, true, false};
    const auto fresh = m.update(frame);
    CHECK(button(fresh.gameplay, Key::left).pressed && fresh.gameplay.pointer.primary.pressed);
}
TEST(binding_capture_cancel_focus_loss_and_keyboard_activation) {
    Menu m;
    // Tab selects the first binding; Enter begins capture, rather than binding Enter itself.
    Input input;
    input.set(Key::tab, true);
    m.update(input.consume());
    input.set(Key::accept, true);
    input.press_symbol(0xff0d);
    m.update(input.consume());
    CHECK(m.settings.capturing_binding());
    InputFrame cancel{};
    cancel[static_cast<std::size_t>(Key::escape)].pressed = true;
    m.update(cancel);
    CHECK(!m.settings.capturing_binding() && m.settings.opened());
    m.click(Settings::binding_first);
    cancel = {};
    cancel.canceled = true;
    cancel.pressed_symbol = 'q';
    CHECK(!m.update(cancel).bindings_changed && !m.settings.capturing_binding());
    CHECK(m.settings.bindings == KeyBindings{});
    m.click(Settings::binding_first);
    cancel = {};
    cancel[static_cast<std::size_t>(Key::settings)].pressed = true;
    m.update(cancel);
    CHECK(!m.settings.opened() && !m.settings.capturing_binding() && !m.game.paused);
}
TEST(controls_layout_stays_inside_small_and_large_viewports) {
    Menu m;
    for (const auto size : {std::pair{320, 240}, {640, 480}, {1280, 720}, {1050, 1360}}) {
        m.width = size.first;
        m.height = size.second;
        m.update();
        CHECK(m.settings.ui().widgets().size() == 12);
        float previous_bottom = 0;
        for (const auto& widget : m.settings.ui().widgets()) {
            CHECK(widget.bounds.min.x >= 0 && widget.bounds.max.x <= static_cast<float>(m.width));
            CHECK(widget.bounds.min.y >= previous_bottom &&
                  widget.bounds.max.y <= static_cast<float>(m.height));
            previous_bottom = widget.bounds.max.y;
        }
    }
}
} // namespace
int main() {
    return testing::run_tests();
}
