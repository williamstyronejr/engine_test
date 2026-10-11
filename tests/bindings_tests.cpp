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
TEST(extended_keyboard_symbols_labels_and_conflicts) {
    KeyBindings bindings;
    for (std::uint32_t symbol = '0'; symbol <= '9'; ++symbol) {
        bindings.assign(6, symbol);
        CHECK(bindings.action(symbol) == Key::interact);
        CHECK(bindings.key_name(Key::interact) == std::string(1, static_cast<char>(symbol)));
    }
    const std::array<NamedBindingSymbol, 17> expected{{{'`', "GRAVE"},
                                                       {'[', "LBRK"},
                                                       {']', "RBRK"},
                                                       {'\\', "BSLS"},
                                                       {';', "SEMI"},
                                                       {'\'', "APOS"},
                                                       {',', "COMMA"},
                                                       {'.', "DOT"},
                                                       {'/', "SLASH"},
                                                       {0xff08, "BKSP"},
                                                       {0xff50, "HOME"},
                                                       {0xff57, "END"},
                                                       {0xff63, "INS"},
                                                       {0xffff, "DEL"},
                                                       {0xffc1, "F4"},
                                                       {0xffc6, "F9"},
                                                       {0xffc9, "F12"}}};
    for (const auto& entry : expected) {
        bindings.assign(6, entry.symbol);
        CHECK(bindings.action(entry.symbol) == Key::interact);
        CHECK(bindings.label(6) == "INTERACT : " + std::string(entry.name));
        const auto before = bindings;
        rejects([&] { bindings.assign(0, entry.symbol); });
        CHECK(bindings == before);
        Configuration config;
        config.bindings = bindings;
        CHECK(decode_config(encode_config(config)) == config);
    }
}
TEST(reserved_and_unsupported_keyboard_symbols_stay_unbindable) {
    KeyBindings bindings;
    const auto before = bindings;
    // Navigation, zoom, diagnostics and modifiers must remain accessible.
    for (const auto symbol :
         {0U,          0x20U,       0x2bU,   0x2dU,   0x3dU,      0x5fU,   0xff1bU, 0xff09U,
          0xfe20U,     0xff0dU,     0xff8dU, 0xff51U, 0xff52U,    0xff53U, 0xff54U, 0xff55U,
          0xff56U,     0xffbeU,     0xffbfU, 0xffc0U, 0xffc2U,    0xffc3U, 0xffc4U, 0xffc5U,
          0xffc7U,     0xffc8U,     0xffe1U, 0xffe2U, 0xffe3U,    0xffe9U, 0xffebU, 0xffb1U,
          0x1008ff12U, 0x010003b1U, 0x41U,   0x21U,   0xffffffffU}) {
        CHECK(!is_bindable_keyboard_symbol(symbol));
        rejects([&] { bindings.assign(6, symbol); });
        rejects([&] { KeyBindings::symbol_name(symbol); });
        CHECK(bindings == before);
    }
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
TEST(configuration_v5_roundtrip_and_legacy_migration) {
    Configuration config;
    config.bindings.assign(0, 'q');
    auto bytes = encode_config(config);
    CHECK(bytes.size() == 112 && bytes[4] == 5 && decode_config(bytes) == config);
    binary::Writer legacy("ECFG");
    legacy.f32(0.5F);
    legacy.f32(0.25F);
    legacy.f32(0.75F);
    legacy.u32(1);
    legacy.u32(0);
    const auto restored = decode_config(seal_record(legacy.take()));
    CHECK(restored.audio.master == 0.5F && restored.audio.muted && !restored.vsync);
    CHECK(restored.bindings == KeyBindings{});
    const auto old_record = [](Configuration value, std::uint8_t version) {
        auto encoded = encode_config(value);
        encoded.resize(68); // Older schemas omit the ten appended modifier masks.
        encoded[4] = version;
        return seal_record(encoded);
    };
    CHECK(decode_config(old_record(config, 2)) == config);
    config.bindings.assign(6, mouse_symbol(MouseButton::right));
    CHECK(decode_config(old_record(config, 3)) == config);
    rejects([&] { decode_config(old_record(config, 2)); });
    config.bindings.assign(0, 0xff50);
    CHECK(decode_config(old_record(config, 4)) == config);
    rejects([&] { decode_config(old_record(config, 3)); });
    config.bindings.assign(0, 'e', modifiers::control);
    config.bindings.assign(6, 'e'); // Plain + chord sharing a base is valid in v5.
    CHECK(decode_config(encode_config(config)) == config);
    rejects([&] { decode_config(old_record(config, 4)); }); // Old duplicate rule retained.
    auto payload = encode_config(config);
    payload.resize(payload.size() - 4);
    payload[68] = 8; // Unsupported modifier bit, valid CRC.
    rejects([&] { decode_config(seal_record(payload)); });
    payload[68] = 0; // Identical plain E assignments.
    rejects([&] { decode_config(seal_record(payload)); });
    payload[68] = 2;
    payload[68 + 6 * 4] = 1; // Ctrl+E and Shift+E overlap ambiguously.
    rejects([&] { decode_config(seal_record(payload)); });
    payload[68 + 6 * 4] = 0;
    payload[28] = 0;
    rejects([&] { decode_config(seal_record(payload)); });
    payload[28] = 'e';
    payload[5] = 1;
    rejects([&] { decode_config(seal_record(payload)); });
}
TEST(chord_specificity_conflicts_and_labels) {
    KeyBindings bindings;
    bindings.assign(0, 'e', modifiers::control);
    bindings.assign(1, 'e', modifiers::control | modifiers::shift);
    bindings.assign(2, 'e', modifiers::all);
    CHECK(bindings.action('e') == Key::interact);
    CHECK(bindings.action('e', modifiers::shift) == Key::interact);
    CHECK(bindings.action('e', modifiers::control | modifiers::alt) == Key::left);
    CHECK(bindings.action('e', modifiers::control | modifiers::shift) == Key::right);
    CHECK(bindings.action('e', modifiers::all) == Key::up);
    CHECK(bindings.key_name(Key::up) == "C+S+A+E");
    const auto before = bindings;
    rejects([&] { bindings.assign(3, 'e', modifiers::alt); });
    rejects([&] { bindings.assign(3, 'e', modifiers::control); });
    rejects([&] { bindings.assign(3, 'q', 8); });
    rejects([&] { bindings.assign(3, 0xffbe, modifiers::control); });
    CHECK(bindings == before);
    bindings.assign(3, mouse_symbol(MouseButton::right), modifiers::alt);
    CHECK(!bindings.action(mouse_symbol(MouseButton::right)));
    CHECK(bindings.action(mouse_symbol(MouseButton::right), modifiers::all) == Key::down);
    CHECK(bindings.key_name(Key::down) == "A+RMB");
    bindings.assign(3, 's');
    CHECK(bindings.modifier_masks[3] == 0);
}
TEST(raw_chord_snapshot_is_atomic_and_cleared_with_capture) {
    Input input;
    input.press_symbol('e', modifiers::control);
    input.press_symbol('q', modifiers::shift);
    auto frame = input.consume();
    CHECK(frame.pressed_symbol == 'e' && frame.pressed_modifiers == modifiers::control);
    CHECK(!input.consume().pressed_modifiers);
    InputGate gate;
    CHECK(!gate.route(frame, true, false).pressed_modifiers);
    input.press_symbol(mouse_symbol(MouseButton::right), modifiers::alt);
    CHECK(!gate.route(input.consume(), false, true).pressed_modifiers);
    input.press_symbol('q', modifiers::all);
    input.release_all();
    frame = input.consume();
    CHECK(!frame.pressed_symbol && !frame.pressed_modifiers);
    rejects([&] { input.press_symbol('q', 8); });
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
TEST(settings_capture_extended_keys_rejects_reserved_and_restores_defaults) {
    Menu m;
    for (const auto symbol :
         {static_cast<std::uint32_t>('1'), static_cast<std::uint32_t>('['), 0xff08U, 0xffc9U}) {
        m.click(Settings::binding_first + 6);
        InputFrame frame{};
        frame.pressed_symbol = 0xffbe; // Reserved F1 symbol; no toggle action in this CPU fixture.
        CHECK(!m.update(frame).bindings_changed && m.settings.capturing_binding());
        CHECK(m.settings.notice == "KEY RESERVED OR UNSUPPORTED");
        frame.pressed_symbol = symbol;
        const auto result = m.update(frame);
        CHECK(result.bindings_changed && !m.settings.capturing_binding());
        CHECK(!result.gameplay.pressed_symbol &&
              m.settings.bindings.action(symbol) == Key::interact);
    }
    CHECK(m.click(Settings::defaults).bindings_changed);
    CHECK(m.settings.bindings == KeyBindings{});
}
TEST(settings_capture_chords_and_conflicts) {
    Menu m;
    m.click(Settings::binding_first);
    InputFrame frame{};
    frame.pressed_symbol = 'e';
    frame.pressed_modifiers = modifiers::control;
    CHECK(m.update(frame).bindings_changed);
    CHECK(m.settings.bindings.key_name(Key::left) == "C+E");
    m.click(Settings::binding_first + 1);
    CHECK(!m.update(frame).bindings_changed && m.settings.capturing_binding());
    frame.pressed_modifiers = modifiers::shift;
    CHECK(!m.update(frame).bindings_changed && m.settings.notice == "AMBIGUOUS CHORDS");
    frame.pressed_modifiers = modifiers::control | modifiers::shift;
    const auto result = m.update(frame);
    CHECK(result.bindings_changed && !result.gameplay.pressed_modifiers);
    CHECK(m.settings.bindings.key_name(Key::right) == "C+S+E");
    CHECK(m.click(Settings::defaults).bindings_changed && m.settings.bindings == KeyBindings{});
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
