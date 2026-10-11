#pragma once
#include "engine/input.hpp"
#include <array>
#include <bit>
#include <optional>
#include <string>
#include <string_view>

namespace engine {
// Stable persisted order. Letter symbols use lowercase ASCII, independent of X11 keycodes.
inline constexpr std::array bindable_actions{
    Key::left,    Key::right,    Key::up,   Key::down,         Key::pause,
    Key::restart, Key::interact, Key::mute, Key::music_volume, Key::effects_volume};
inline constexpr std::array<std::string_view, 10> binding_names{
    "MOVE LEFT", "MOVE RIGHT", "MOVE UP", "MOVE DOWN",    "PAUSE",
    "RESTART",   "INTERACT",   "MUTE",    "MUSIC VOLUME", "EFFECTS VOLUME"};
// Stable X11 key symbols, without an Xlib dependency in the CPU core.
// Short ASCII labels also work in the engine's bitmap font and compact HUD.
struct NamedBindingSymbol {
    std::uint32_t symbol;
    std::string_view name;
};
inline constexpr std::array<NamedBindingSymbol, 17> named_binding_symbols{
    {NamedBindingSymbol{'`', "GRAVE"},
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
inline constexpr bool is_letter_symbol(std::uint32_t symbol) {
    return symbol >= 'a' && symbol <= 'z';
}
inline constexpr bool is_bindable_keyboard_symbol(std::uint32_t symbol) {
    if (is_letter_symbol(symbol) || (symbol >= '0' && symbol <= '9'))
        return true;
    for (const auto& entry : named_binding_symbols)
        if (entry.symbol == symbol)
            return true;
    return false;
}
struct KeyBindings {
    std::array<std::uint32_t, 10> symbols{'a', 'd', 'w', 's', 'p', 'r', 'e', 'm', 'n', 'b'};
    std::array<std::uint32_t, 10> modifier_masks{};
    bool operator==(const KeyBindings&) const = default;
    void validate() const {
        for (std::size_t i = 0; i < symbols.size(); ++i) {
            const auto symbol = symbols[i];
            if (!is_bindable_keyboard_symbol(symbol) && !is_mouse_symbol(symbol))
                throw std::invalid_argument("KEY RESERVED OR UNSUPPORTED");
            const auto mask = modifier_masks[i];
            if (mask & ~modifiers::all)
                throw std::invalid_argument("UNSUPPORTED MODIFIER");
            for (std::size_t j = 0; j < i; ++j) {
                if (symbols[j] != symbol)
                    continue;
                const auto other = modifier_masks[j];
                if (mask == other)
                    throw std::invalid_argument("INPUT ALREADY ASSIGNED");
                if ((mask & other) != mask && (mask & other) != other)
                    throw std::invalid_argument("AMBIGUOUS CHORDS");
            }
        }
    }
    void assign(std::size_t action, std::uint32_t symbol, std::uint32_t mask = 0) {
        auto candidate = *this;
        candidate.symbols.at(action) = symbol;
        candidate.modifier_masks.at(action) = mask;
        candidate.validate();
        *this = candidate;
    }
    // Valid tables have nested masks for shared symbols, so specificity has no ties.
    std::optional<Key> action(std::uint32_t symbol, std::uint32_t held_modifiers = 0) const {
        std::optional<Key> result;
        int specificity = -1;
        for (std::size_t i = 0; i < symbols.size(); ++i) {
            const auto mask = modifier_masks[i];
            if (symbols[i] == symbol && (held_modifiers & mask) == mask &&
                std::popcount(mask) > specificity) {
                result = bindable_actions[i];
                specificity = std::popcount(mask);
            }
        }
        return result;
    }
    static std::string symbol_name(std::uint32_t symbol) {
        if (is_mouse_symbol(symbol)) {
            constexpr std::array names{"LMB", "MMB", "RMB", "SIDE1", "SIDE2"};
            return names[symbol - mouse_symbol(MouseButton::left)];
        }
        if (is_letter_symbol(symbol))
            return std::string(1, static_cast<char>(symbol - 'a' + 'A'));
        if (symbol >= '0' && symbol <= '9')
            return std::string(1, static_cast<char>(symbol));
        for (const auto& entry : named_binding_symbols)
            if (entry.symbol == symbol)
                return std::string(entry.name);
        throw std::invalid_argument("Invalid binding symbol");
    }
    std::string chord_name(std::size_t index) const {
        const auto mask = modifier_masks.at(index);
        std::string name;
        if (mask & modifiers::control)
            name += "C+";
        if (mask & modifiers::shift)
            name += "S+";
        if (mask & modifiers::alt)
            name += "A+";
        return name + symbol_name(symbols.at(index));
    }
    std::string key_name(Key key) const {
        for (std::size_t i = 0; i < bindable_actions.size(); ++i)
            if (bindable_actions[i] == key)
                return chord_name(i);
        throw std::invalid_argument("Action has no configurable binding");
    }
    std::string label(std::size_t index) const {
        return std::string(binding_names.at(index)) + " : " + chord_name(index);
    }
};
} // namespace engine
