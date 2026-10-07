#pragma once
#include "engine/binary.hpp"
#include "settings.hpp"

namespace feature_lab {
struct Configuration {
    AudioSettings audio;
    bool vsync{true};
    engine::KeyBindings bindings{};
    bool operator==(const Configuration&) const = default;
};
inline void validate(Configuration config) {
    config.bindings.validate();
    for (float value : {config.audio.master, config.audio.music, config.audio.effects})
        if (!std::isfinite(value) || value < 0 || value > 1)
            throw std::runtime_error("Configuration gain outside [0,1]");
}
inline std::vector<std::uint8_t> encode_config(Configuration config) {
    validate(config);
    engine::binary::Writer out("ECFG", 2);
    out.f32(config.audio.master);
    out.f32(config.audio.music);
    out.f32(config.audio.effects);
    out.u32(config.audio.muted);
    out.u32(config.vsync);
    for (const auto symbol : config.bindings.letters)
        out.u32(symbol);
    return engine::seal_record(out.take());
}
inline Configuration decode_config(std::span<const std::uint8_t> bytes) {
    const auto payload = engine::open_record(bytes);
    if (payload.size() < 8)
        throw std::runtime_error("Truncated configuration");
    const auto version = static_cast<std::uint32_t>(payload[4]);
    if (version != 1 && version != 2)
        throw std::runtime_error("Unsupported configuration version");
    engine::binary::Reader in(payload, "ECFG", version);
    Configuration config{{in.f32(), in.f32(), in.f32(), in.boolean()}, in.boolean()};
    if (version == 2)
        for (auto& symbol : config.bindings.letters)
            symbol = in.u32();
    in.finish();
    validate(config);
    return config;
}
inline std::vector<std::uint8_t> encode_checkpoint(const Checkpoint& state) {
    if (state.collected_ids.size() > 64)
        throw std::invalid_argument("Checkpoint core limit exceeded");
    engine::binary::Writer out("ESAV", 2);
    for (auto part : state.content)
        out.u32(part);
    out.f32(state.position.x);
    out.f32(state.position.y);
    out.f32(state.camera_height);
    out.u64(state.ticks);
    out.u32(state.paused);
    out.u32(state.won);
    out.u32(state.door_started);
    out.u32(state.door_open);
    out.u32(static_cast<std::uint32_t>(state.collected_ids.size()));
    for (auto id : state.collected_ids)
        out.u64(id);
    for (const auto& animation : state.animations) {
        if (animation.clip.size() > 80)
            throw std::invalid_argument("Checkpoint clip name too long");
        out.string(animation.clip);
        out.u64(animation.position);
        out.u32(animation.paused);
    }
    out.u64(state.alarm_entries);
    out.u32(state.alarm_disabled);
    return engine::seal_record(out.take());
}
inline Checkpoint decode_checkpoint(std::span<const std::uint8_t> bytes) {
    engine::binary::Reader in(engine::open_record(bytes), "ESAV", 2);
    Checkpoint state;
    for (auto& part : state.content)
        part = in.u32();
    state.position = {in.f32(), in.f32()};
    state.camera_height = in.f32();
    state.ticks = in.u64();
    state.paused = in.boolean();
    state.won = in.boolean();
    state.door_started = in.boolean();
    state.door_open = in.boolean();
    const auto count = in.u32();
    if (count > 64)
        throw std::runtime_error("Checkpoint core limit exceeded");
    for (std::uint32_t i = 0; i < count; ++i)
        state.collected_ids.push_back(in.u64());
    for (auto& animation : state.animations) {
        animation.clip = in.string(80);
        animation.position = in.u64();
        animation.paused = in.boolean();
    }
    state.alarm_entries = in.u64();
    state.alarm_disabled = in.boolean();
    in.finish();
    return state;
}
inline void save_slot(const engine::UserStorage& storage, unsigned slot, const Game& game,
                      bool gameplay_paused) {
    auto state = game.checkpoint();
    state.paused = gameplay_paused; // Menu pause is transient UI state.
    game.validate_checkpoint(state);
    storage.write_slot(slot, encode_checkpoint(state));
}
inline void load_slot(const engine::UserStorage& storage, unsigned slot, Game& game) {
    const auto bytes = storage.read_slot(slot);
    if (!bytes)
        throw std::runtime_error("Save slot is empty");
    game.restore(decode_checkpoint(*bytes));
}
} // namespace feature_lab
