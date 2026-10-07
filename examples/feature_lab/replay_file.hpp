#pragma once
#include "persistence.hpp"
#include <bit>
#include <limits>

namespace feature_lab {
// The array order is part of ERPL v1; it is independent of native keys and Key ordinals.
inline constexpr std::array replay_actions{
    engine::Key::left,    engine::Key::right,       engine::Key::up,      engine::Key::down,
    engine::Key::zoom_in, engine::Key::zoom_out,    engine::Key::pause,   engine::Key::space,
    engine::Key::restart, engine::Key::single_step, engine::Key::interact};
inline constexpr std::size_t max_replay_commands = 36000;
inline constexpr std::size_t max_replay_bytes = 1024 * 1024;
struct ReplayCommand {
    std::uint64_t tick_before{};
    std::uint32_t held{}, pressed{}, released{};
    std::uint64_t state_hash{};
    engine::InputFrame input() const {
        constexpr auto allowed = (1U << replay_actions.size()) - 1;
        if (((held | pressed | released) & ~allowed) != 0 ||
            tick_before > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            throw std::runtime_error("Invalid replay command");
        engine::InputFrame result{};
        for (std::size_t i = 0; i < replay_actions.size(); ++i) {
            const auto mask = 1U << i;
            result[static_cast<std::size_t>(replay_actions[i])] = {
                (held & mask) != 0, (pressed & mask) != 0, (released & mask) != 0};
        }
        return result;
    }
    static ReplayCommand capture(const Game& game, const engine::InputFrame& input) {
        ReplayCommand result;
        result.tick_before = game.ticks;
        for (std::size_t i = 0; i < replay_actions.size(); ++i) {
            const auto& value = engine::button(input, replay_actions[i]);
            result.held |= static_cast<std::uint32_t>(value.held) << i;
            result.pressed |= static_cast<std::uint32_t>(value.pressed) << i;
            result.released |= static_cast<std::uint32_t>(value.released) << i;
        }
        return result;
    }
};
// Canonical little-endian FNV-1a of authoritative mutable simulation state.
// No checkpoint encoding, content rehashing, or allocation in the per-step path.
inline std::uint64_t replay_hash(const Game& game) {
    std::uint64_t hash = 14695981039346656037ULL;
    const auto byte = [&](std::uint8_t value) { hash = (hash ^ value) * 1099511628211ULL; };
    const auto integer = [&](std::uint64_t value) {
        for (unsigned i = 0; i < 8; ++i)
            byte(static_cast<std::uint8_t>(value >> (8 * i)));
    };
    integer(std::bit_cast<std::uint32_t>(game.position.x));
    integer(std::bit_cast<std::uint32_t>(game.position.y));
    integer(std::bit_cast<std::uint32_t>(game.camera_height));
    integer(game.ticks);
    integer(game.paused);
    integer(game.won);
    integer(game.door_started);
    integer(game.door_open);
    integer(game.door_completions);
    integer(game.alarm_entries);
    integer(game.alarm_disabled);
    integer(game.alarm_touching);
    integer(game.random_seed);
    integer(game.random.state());
    integer(game.collected.size());
    for (std::size_t i = 0; i < game.collected.size(); ++i) {
        integer(game.core_ids[i]);
        integer(game.collected[i]);
    }
    for (const auto* animation : {&game.player_animation, &game.core_animation,
                                  &game.machine_animation, &game.door_animation}) {
        integer(animation->clip_name().size());
        for (const auto ch : animation->clip_name())
            byte(static_cast<std::uint8_t>(ch));
        integer(animation->position());
        integer(animation->paused());
    }
    return hash;
}
struct ReplayFile {
    Checkpoint initial;
    std::vector<ReplayCommand> commands;
};
inline std::vector<std::uint8_t> encode_replay(const ReplayFile& file) {
    if (file.commands.size() > max_replay_commands)
        throw std::runtime_error("Replay command limit exceeded");
    const auto checkpoint = encode_checkpoint(file.initial);
    engine::binary::Writer out("ERPL");
    out.u32(1); // Feature Lab simulation rules version.
    out.u32(static_cast<std::uint32_t>(file.commands.size()));
    out.u32(static_cast<std::uint32_t>(checkpoint.size()));
    out.bytes(checkpoint);
    for (const auto& command : file.commands) {
        command.input(); // Validate even caller-constructed records.
        out.u64(command.tick_before);
        out.u32(command.held);
        out.u32(command.pressed);
        out.u32(command.released);
        out.u64(command.state_hash);
    }
    return engine::seal_record(out.take(), max_replay_bytes);
}
inline ReplayFile decode_replay(std::span<const std::uint8_t> bytes) {
    engine::binary::Reader in(engine::open_record(bytes, max_replay_bytes), "ERPL");
    if (in.u32() != 1)
        throw std::runtime_error("Unsupported replay simulation rules");
    const auto count = in.u32();
    if (count > max_replay_commands)
        throw std::runtime_error("Replay command limit exceeded");
    const auto size = in.u32();
    if (size > engine::max_record_bytes)
        throw std::runtime_error("Replay checkpoint limit exceeded");
    ReplayFile result{decode_checkpoint(in.bytes(size)), {}};
    // Prove the complete payload exists before reserving command storage.
    in.require(static_cast<std::size_t>(count) * 28);
    result.commands.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        ReplayCommand command{in.u64(), in.u32(), in.u32(), in.u32(), in.u64()};
        command.input();
        result.commands.push_back(command);
    }
    in.finish();
    return result;
}
inline ReplayFile read_replay(const std::filesystem::path& path) {
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    return decode_replay(engine::AssetRoot(absolute.parent_path())
                             .read(absolute.filename().string(), max_replay_bytes));
}
inline void write_replay(const std::filesystem::path& path, const ReplayFile& file) {
    const auto bytes = encode_replay(file);
    const auto absolute = std::filesystem::absolute(path).lexically_normal();
    engine::AssetRoot(absolute.parent_path()).write_atomic(absolute.filename().string(), bytes);
}
class ReplayRecorder {
  public:
    explicit ReplayRecorder(const Game& game) : file_{game.checkpoint(), {}} {
        file_.commands.reserve(max_replay_commands);
    }
    bool full() const { return file_.commands.size() == max_replay_commands; }
    const ReplayFile& file() const { return file_; }
    int step(Game& game, const engine::InputFrame& input) {
        if (full())
            throw std::runtime_error("Replay recording is full");
        auto command = ReplayCommand::capture(game, input);
        const auto picked = game.update(command.input());
        command.state_hash = replay_hash(game);
        file_.commands.push_back(command);
        return picked;
    }

  private:
    ReplayFile file_;
};
inline int playback_step(Game& game, const ReplayFile& file, std::size_t index) {
    const auto& command = file.commands.at(index);
    const auto fail = [&] {
        throw std::runtime_error("Replay diverged at command " + std::to_string(index) +
                                 " (expected pre-update tick " +
                                 std::to_string(command.tick_before) + ")");
    };
    if (game.ticks != command.tick_before)
        fail();
    const auto picked = game.update(command.input());
    if (replay_hash(game) != command.state_hash)
        fail();
    return picked;
}
} // namespace feature_lab
