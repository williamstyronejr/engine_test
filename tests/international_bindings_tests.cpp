#include "../examples/feature_lab/persistence.hpp"
#include "test.hpp"

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
TEST(legacy_and_unicode_character_aliases_have_one_identity) {
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 10> aliases{{{0x010000e9, 0xe9},
                                                                           {0x01b1, 0x01000105},
                                                                           {0x07e1, 0x010003b1},
                                                                           {0x06d1, 0x0100044f},
                                                                           {0x05c7, 0x01000627},
                                                                           {0x0ce0, 0x010005d0},
                                                                           {0x04b1, 0x010030a2},
                                                                           {0x0da1, 0x01000e01},
                                                                           {0x20ac, 0x010020ac},
                                                                           {0x01000051, 'q'}}};
    for (const auto& [raw, canonical] : aliases) {
        CHECK(canonical_character_symbol(raw) == canonical);
        CHECK(canonical_character_symbol(canonical) == canonical);
        KeyBindings b;
        b.assign(6, canonical, modifiers::control);
        CHECK(b.action(canonical_character_symbol(raw), modifiers::control) == Key::interact);
        rejects([&] { b.assign(0, canonical_character_symbol(raw), modifiers::control); });
        if (raw != canonical)
            rejects([&] { b.assign(6, raw); }); // Persisted/API tables must be canonical.
    }
    for (const auto function : {0xff51U, 0xffbeU, 0xfe51U, 0xff8dU, 0x1008ff12U})
        CHECK(canonical_character_symbol(function) == function);
}
TEST(international_vocabulary_and_ascii_labels) {
    const std::array<std::pair<std::uint32_t, const char*>, 8> examples{{{0xe9, "EACUTE"},
                                                                         {0xe4, "AUML"},
                                                                         {0xc9, "U+00C9"},
                                                                         {0x010003b1, "U+03B1"},
                                                                         {0x0100044f, "U+044F"},
                                                                         {0x01004e2d, "U+4E2D"},
                                                                         {0x0101f600, "U+1F600"},
                                                                         {0x01020000, "U+20000"}}};
    for (const auto& [symbol, name] : examples) {
        CHECK(is_international_binding_symbol(symbol));
        KeyBindings b;
        b.assign(6, symbol, modifiers::all);
        CHECK(b.key_name(Key::interact) == "C+S+A+" + std::string(name));
        for (const auto c : b.label(6))
            CHECK((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == ':' ||
                  c == '+');
        Configuration config;
        config.bindings = b;
        CHECK(decode_config(encode_config(config)) == config);
    }
    // Non-ASCII case is intentionally preserved, not locale-sensitive folded.
    KeyBindings b;
    b.assign(0, 0xe9);
    b.assign(1, 0xc9);
    CHECK(b.action(0xe9) == Key::left && b.action(0xc9) == Key::right);
}
TEST(noncharacters_and_non_graphic_unicode_are_rejected) {
    for (const auto symbol :
         {0xa0U, 0xadU, 0x010000e9U, 0x07e1U, 0x01000020U, 0x01000301U, 0x01000378U, 0x0100200bU,
          0x0100202eU, 0x0100d800U, 0x0100dfffU, 0x0100e000U, 0x0100fdd0U, 0x0100ffffU, 0x0110ffffU,
          0x01110000U, 0xffffffffU}) {
        CHECK(!is_international_binding_symbol(symbol));
        KeyBindings b;
        const auto original = b;
        rejects([&] { b.assign(6, symbol); });
        rejects([&] { KeyBindings::symbol_name(symbol); });
        CHECK(b == original);
    }
}
TEST(layout_punctuation_has_labels_and_keeps_fixed_controls) {
    KeyBindings b;
    for (const auto& entry : extra_binding_symbols) {
        b.assign(6, entry.symbol);
        CHECK(b.key_name(Key::interact) == entry.name);
        CHECK(b.action(entry.symbol) == Key::interact);
        CHECK(!is_v6_bindable_keyboard_symbol(entry.symbol));
    }
    for (const auto symbol : {' ', '+', '-', '=', '_'})
        rejects([&] { b.assign(6, static_cast<std::uint32_t>(symbol)); });
}
TEST(v7_migration_preserves_v6_but_rejects_new_symbols_in_old_records) {
    const auto record = [](Configuration config, std::uint8_t version) {
        auto bytes = encode_config(config);
        bytes.resize(bytes.size() - 4);
        bytes[4] = version;
        if (version < 5)
            bytes.resize(68);
        return seal_record(bytes);
    };
    Configuration old;
    old.bindings.assign(6, 0xffb1, modifiers::control);
    CHECK(decode_config(record(old, 6)) == old);
    CHECK(encode_config(old).size() == 112 && encode_config(old)[4] == 7);
    for (const auto symbol : {0xe9U, 0x010003b1U, 0x21U}) {
        Configuration config;
        config.bindings.assign(6, symbol);
        CHECK(decode_config(record(config, 7)) == config);
        for (std::uint8_t version = 2; version < 7; ++version)
            rejects([&] { decode_config(record(config, version)); });
    }
    auto malformed = encode_config(old);
    malformed.resize(malformed.size() - 4);
    // A valid CRC must not permit a surrogate or noncanonical legacy alias.
    for (const auto invalid : {0x0100d800U, 0x07e1U, 0x010000e9U}) {
        for (unsigned byte = 0; byte < 4; ++byte)
            malformed[28 + byte] = static_cast<std::uint8_t>(invalid >> (byte * 8));
        rejects([&] { decode_config(seal_record(malformed)); });
    }
}
} // namespace
int main() {
    return testing::run_tests();
}
