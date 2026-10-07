#include "../examples/feature_lab/replay_file.hpp"
#include "test.hpp"
#include <unistd.h>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
auto game() {
    return load_game(AssetRoot(TEST_ASSET_ROOT));
}
auto reseal(std::vector<std::uint8_t> bytes) {
    bytes.resize(bytes.size() - 4);
    return seal_record(std::move(bytes), max_replay_bytes);
}
TEST(random_reference_vectors_jump_and_restore) {
    Random random(0);
    CHECK(random.next() == 0xe220a8397b1dcdafULL);
    CHECK(random.next() == 0x6e789e6aa1b965f4ULL);
    CHECK(random.next() == 0x06c45d188009454fULL);
    const auto saved = random.state();
    const auto next = random.next();
    random.restore(saved);
    CHECK(random.next() == next);
    Random jump(0);
    jump.advance(4);
    CHECK(jump.state() == random.state());
    Random wrap(UINT64_MAX);
    const auto first = wrap.next();
    wrap.restore(UINT64_MAX);
    CHECK(wrap.next() == first);
}
TEST(checkpoint_v2_migrates_random_state_and_v3_preserves_seed) {
    auto source = game();
    for (int i = 0; i < 123; ++i)
        source.update({});
    auto legacy = encode_checkpoint(source.checkpoint());
    legacy.resize(legacy.size() - 20); // Remove v3 random fields and CRC.
    legacy[4] = 2;
    legacy = seal_record(std::move(legacy));
    const auto decoded = decode_checkpoint(legacy);
    CHECK(decoded.random_seed == 1);
    CHECK(decoded.random_state == source.random.state());
    auto restored = game();
    restored.restore(decoded);
    CHECK(replay_hash(source) == replay_hash(restored));
    source.random_seed = UINT64_MAX;
    source.random.restore(9876);
    restored.restore(decode_checkpoint(encode_checkpoint(source.checkpoint())));
    CHECK(replay_hash(source) == replay_hash(restored));
    InputFrame restart{};
    restart[static_cast<std::size_t>(Key::restart)].pressed = true;
    restored.update(restart);
    CHECK(restored.random.state() == UINT64_MAX);
}
TEST(full_game_file_roundtrip_replays_every_state) {
    auto source = game();
    source.random_seed = 123456;
    source.random.restore(source.random_seed);
    ReplayRecorder recording(source);
    Replay route;
    for (int i = 0; i < 1600; ++i)
        recording.step(source, route.next(source));
    CHECK(source.won);
    CHECK(source.count() == 3);
    CHECK(source.door_completions == 1);
    const auto bytes = encode_replay(recording.file());
    const auto file = decode_replay(bytes);
    CHECK(encode_replay(file) == bytes);
    auto playback = game();
    playback.restore(file.initial);
    for (std::size_t i = 0; i < file.commands.size(); ++i)
        playback_step(playback, file, i);
    CHECK(encode_checkpoint(playback.checkpoint()) == encode_checkpoint(source.checkpoint()));
}
TEST(paused_initial_checkpoint_edges_restart_step_and_seed_divergence) {
    auto source = game();
    for (int i = 0; i < 75; ++i)
        source.update({});
    source.paused = true;
    ReplayRecorder recording(source);
    recording.step(source, {});
    for (const auto key : {Key::single_step, Key::space, Key::zoom_in, Key::right, Key::interact,
                           Key::pause, Key::single_step, Key::restart}) {
        InputFrame input{};
        input[static_cast<std::size_t>(key)] = {true, true, true};
        recording.step(source, input);
    }
    InputFrame tap{};
    tap[static_cast<std::size_t>(Key::pause)] = {false, true, true};
    recording.step(source, tap);
    const auto command = ReplayCommand::capture(source, tap);
    CHECK(button(command.input(), Key::pause).pressed);
    CHECK(button(command.input(), Key::pause).released);
    CHECK(!button(command.input(), Key::pause).held);
    const auto file = decode_replay(encode_replay(recording.file()));
    auto playback = game();
    playback.restore(file.initial);
    for (std::size_t i = 0; i < file.commands.size(); ++i)
        playback_step(playback, file, i);
    CHECK(replay_hash(source) == replay_hash(playback));
    playback.restore(file.initial);
    playback.random.restore(99);
    rejects([&] { playback_step(playback, file, 0); });
}
TEST(replay_parser_rejects_corruption_versions_bounds_and_trailing_data) {
    auto source = game();
    ReplayRecorder recording(source);
    recording.step(source, {});
    const auto bytes = encode_replay(recording.file());
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        auto bad = bytes;
        bad[i] ^= 1;
        rejects([&] { decode_replay(bad); });
        rejects([&] { decode_replay(std::span(bytes).first(i)); });
    }
    for (const auto offset : {0U, 4U, 8U, 12U, 16U}) {
        auto bad = bytes;
        bad[offset] = 255;
        bad[offset + 1] = 255;
        rejects([&] { decode_replay(reseal(bad)); });
    }
    auto bad = bytes;
    bad.insert(bad.end() - 4, 0);
    rejects([&] { decode_replay(reseal(bad)); });
    const auto entry = bytes.size() - 4 - 28;
    bad = bytes;
    bad.at(entry + 8 + 3) = 128; // Unknown held bit, with a valid envelope.
    rejects([&] { decode_replay(reseal(bad)); });
    bad = bytes;
    bad.at(entry + 7) = 128; // Invalid tick range.
    rejects([&] { decode_replay(reseal(bad)); });
    rejects([] { decode_replay(std::vector<std::uint8_t>(max_replay_bytes + 1)); });
    rejects([] { seal_record(std::vector<std::uint8_t>(8), 3); });
}
TEST(replay_rejects_content_tick_and_state_divergence) {
    auto source = game();
    ReplayRecorder recording(source);
    recording.step(source, {});
    auto file = recording.file();
    auto playback = game();
    const auto original = playback.world.get(playback.player).entity;
    file.initial.content[0] ^= 1;
    rejects([&] { playback.restore(file.initial); });
    CHECK(playback.world.valid(original));
    file = recording.file();
    file.commands[0].tick_before = 1;
    const auto hash = replay_hash(playback);
    rejects([&] { playback_step(playback, file, 0); });
    CHECK(replay_hash(playback) == hash);
    file = recording.file();
    file.commands[0].state_hash ^= 1;
    try {
        playback_step(playback, file, 0);
        CHECK(false);
    } catch (const std::runtime_error& error) {
        CHECK(std::string(error.what()).find("diverged at command 0") != std::string::npos);
    }
}
TEST(recording_limit_and_empty_replay) {
    auto source = game();
    source.paused = true;
    ReplayRecorder recording(source);
    const auto empty = decode_replay(encode_replay(recording.file()));
    CHECK(empty.commands.empty());
    for (std::size_t i = 0; i < max_replay_commands; ++i)
        recording.step(source, {});
    CHECK(recording.full());
    CHECK(source.ticks == 0);
    rejects([&] { recording.step(source, {}); });
    auto file = decode_replay(encode_replay(recording.file()));
    CHECK(file.commands.size() == max_replay_commands);
    file.commands.push_back({});
    rejects([&] { encode_replay(file); });
}
TEST(replay_atomic_file_roundtrip_and_failed_replacement) {
    std::string pattern =
        (std::filesystem::temp_directory_path() / "engine-replay-XXXXXX").string();
    CHECK(mkdtemp(pattern.data()) != nullptr);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    } cleanup{pattern};
    const auto path = cleanup.path / "recording.erpl";
    auto source = game();
    ReplayRecorder recording(source);
    recording.step(source, {});
    write_replay(path, recording.file());
    const auto original = encode_replay(recording.file());
    CHECK(encode_replay(read_replay(path)) == original);
    auto bad = recording.file();
    bad.commands[0].pressed = 0x80000000U;
    rejects([&] { write_replay(path, bad); });
    CHECK(encode_replay(read_replay(path)) == original);
    rejects([&] { write_replay(cleanup.path / "absent" / "file.erpl", recording.file()); });
}
} // namespace
int main() {
    return run_tests();
}
