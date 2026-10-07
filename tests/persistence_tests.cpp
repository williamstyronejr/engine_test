#include "../examples/feature_lab/persistence.hpp"
#include "test.hpp"
#include <fstream>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
struct Temporary {
    std::filesystem::path path;
    Temporary() {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "engine-save-XXXXXX").string();
        if (!mkdtemp(pattern.data()))
            throw std::runtime_error("Cannot create test directory");
        path = pattern;
    }
    ~Temporary() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    UserStorage storage() const { return UserStorage(UserPaths::discover(path)); }
};
auto game() {
    return load_game(AssetRoot(TEST_ASSET_ROOT));
}
auto reseal(std::vector<std::uint8_t> bytes) {
    bytes.resize(bytes.size() - 4);
    return seal_record(std::move(bytes));
}
TEST(record_crc_reference_bounds_and_corruption) {
    const std::string reference = "123456789";
    CHECK(crc32({reinterpret_cast<const std::uint8_t*>(reference.data()), reference.size()}) ==
          0xcbf43926U);
    const auto bytes = encode_config({});
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        auto bad = bytes;
        bad[i] ^= 1;
        rejects([&] { decode_config(bad); });
        rejects([&] { decode_config(std::span(bytes).first(i)); });
    }
    auto extra = bytes;
    extra.push_back(0);
    rejects([&] { decode_config(extra); });
    rejects([] { seal_record(std::vector<std::uint8_t>(max_record_bytes)); });
    rejects([] { open_record(std::vector<std::uint8_t>(max_record_bytes + 1)); });
}
TEST(config_versions_ranges_and_exact_roundtrip) {
    const Configuration config{{0.1234567F, 0, 1, true}, false};
    CHECK(decode_config(encode_config(config)) == config);
    auto bad = encode_config(config);
    bad[4] = 99;
    bad = reseal(bad);
    rejects([&] { decode_config(bad); });
    bad = encode_config(config);
    bad[20] = 2; // Muted boolean must be exactly 0/1, even with valid checksum.
    bad = reseal(bad);
    rejects([&] { decode_config(bad); });
    rejects([] { encode_config({{NAN, 0, 0, false}, true}); });
    rejects([] { encode_config({{1, -1, 0, false}, true}); });
    bad = encode_config(config);
    bad.insert(bad.end() - 4, 0);
    bad = reseal(bad);
    rejects([&] { decode_config(bad); });
}
TEST(storage_slots_missing_limits_and_relaunch) {
    Temporary tmp;
    const auto storage = tmp.storage();
    CHECK(!storage.read_config() && !storage.read_slot(1));
    CHECK(!std::filesystem::exists(storage.paths().config));
    CHECK(!std::filesystem::exists(storage.paths().state));
    const Configuration config{{0.4F, 0.2F, 0.7F, true}, false};
    storage.write_config(encode_config(config));
    auto current = game();
    for (unsigned slot = 1; slot <= 3; ++slot) {
        current.camera_height = 10 + static_cast<float>(slot);
        save_slot(storage, slot, current, slot == 2);
    }
    const auto reopened = tmp.storage();
    CHECK(decode_config(*reopened.read_config()) == config);
    for (unsigned slot = 1; slot <= 3; ++slot) {
        auto loaded = game();
        load_slot(reopened, slot, loaded);
        NEAR(loaded.camera_height, 10 + static_cast<float>(slot));
        CHECK(loaded.paused == (slot == 2));
    }
    rejects([&] { storage.read_slot(0); });
    rejects([&] { storage.write_slot(4, encode_checkpoint(current.checkpoint())); });
    rejects([&] { storage.write_config({}); });
    CHECK(decode_config(*storage.read_config()) == config);
    std::ofstream oversized(storage.paths().state / "slot-1.esav", std::ios::binary);
    oversized << std::string(max_record_bytes + 1, 'x');
    oversized.close();
    rejects([&] { storage.read_slot(1); });
}
TEST(user_paths_use_absolute_xdg_values_and_home_fallback) {
    struct Environment {
        const char* name;
        std::optional<std::string> old;
        explicit Environment(const char* key) : name(key) {
            if (const auto value = std::getenv(name))
                old = value;
        }
        ~Environment() {
            if (old)
                setenv(name, old->c_str(), 1);
            else
                unsetenv(name);
        }
    } config("XDG_CONFIG_HOME"), state("XDG_STATE_HOME"), home("HOME");
    setenv("HOME", "/test-home", 1);
    setenv("XDG_CONFIG_HOME", "relative-ignored", 1);
    unsetenv("XDG_STATE_HOME");
    auto paths = UserPaths::discover();
    CHECK(paths.config == "/test-home/.config/feature_lab");
    CHECK(paths.state == "/test-home/.local/state/feature_lab");
    setenv("XDG_CONFIG_HOME", "/test-config", 1);
    setenv("XDG_STATE_HOME", "/test-state", 1);
    unsetenv("HOME");
    paths = UserPaths::discover();
    CHECK(paths.config == "/test-config/feature_lab" && paths.state == "/test-state/feature_lab");
    unsetenv("XDG_STATE_HOME");
    rejects([] { UserPaths::discover(); });
    rejects([] { UserStorage invalid({"relative", "/absolute"}); });
}
TEST(storage_failed_save_preserves_previous_slot) {
    Temporary tmp;
    const auto storage = tmp.storage();
    auto current = game();
    save_slot(storage, 1, current, false);
    const auto previous = storage.read_slot(1);
    current.position.x = NAN;
    rejects([&] { save_slot(storage, 1, current, false); });
    CHECK(storage.read_slot(1) == previous);
    Temporary blocked;
    std::ofstream(blocked.path / "file") << "not a directory";
    UserStorage unavailable({blocked.path / "file/config", blocked.path / "file/state"});
    rejects([&] { unavailable.write_config(encode_config({})); });
    CHECK(storage.read_slot(1) == previous);
}
TEST(animation_restore_exact_boundary_and_failure_preservation) {
    auto current = game();
    auto& animation = current.door_animation;
    animation.play("door_open");
    animation.advance(13);
    const auto saved = animation.snapshot();
    animation.advance(100);
    animation.restore(saved);
    CHECK(animation.snapshot() == saved && !animation.finished());
    auto invalid = saved;
    invalid.position = UINT64_MAX;
    rejects([&] { animation.restore(invalid); });
    CHECK(animation.snapshot() == saved);
    invalid.clip = "missing";
    rejects([&] { animation.restore(invalid); });
    CHECK(animation.snapshot() == saved);
    CHECK(animation.advance(19).completed);
    const auto completed = animation.snapshot();
    animation.play("door_open");
    animation.restore(completed);
    CHECK(animation.finished() && !animation.advance(1).completed);
}
TEST(checkpoint_every_replay_phase_resumes_identically) {
    auto original = game();
    auto restored = game();
    restored.restore(decode_checkpoint(encode_checkpoint(original.checkpoint())));
    Replay replay;
    bool partial_door = false, completed = false;
    for (int i = 0; i < 1600 && !original.won; ++i) {
        const auto input = replay.next(original);
        original.update(input);
        restored.update(input);
        CHECK(original.position.x == restored.position.x &&
              original.position.y == restored.position.y);
        CHECK(original.ticks == restored.ticks && original.collected == restored.collected);
        CHECK(original.door_started == restored.door_started &&
              original.door_open == restored.door_open);
        CHECK(original.alarm_entries == restored.alarm_entries &&
              original.alarm_touching == restored.alarm_touching);
        CHECK(original.won == restored.won &&
              original.door_completions == restored.door_completions);
        CHECK(original.player_animation.snapshot() == restored.player_animation.snapshot());
        CHECK(original.core_animation.snapshot() == restored.core_animation.snapshot());
        CHECK(original.machine_animation.snapshot() == restored.machine_animation.snapshot());
        CHECK(original.door_animation.snapshot() == restored.door_animation.snapshot());
        if (i % 113 != 0 && !(original.door_started && !partial_door) && !original.won)
            continue;
        const auto bytes = encode_checkpoint(original.checkpoint());
        const auto old_player = restored.player;
        restored.restore(decode_checkpoint(bytes));
        CHECK(!restored.world.valid(old_player));
        CHECK(encode_checkpoint(restored.checkpoint()) == bytes);
        NEAR(restored.previous.x, restored.position.x);
        NEAR(restored.previous.y, restored.position.y);
        if (original.door_started && !original.door_open)
            partial_door = true;
        completed = original.won;
    }
    CHECK(partial_door && completed && original.door_completions == 1);
    CHECK(!restored.door_animation.advance(1).completed);
}
TEST(checkpoint_invalid_state_never_replaces_live_game) {
    auto live = game();
    Replay replay;
    for (int i = 0; i < 400; ++i)
        live.update(replay.next(live));
    const auto before = encode_checkpoint(live.checkpoint());
    const auto player = live.player;
    const auto reject = [&](auto change) {
        auto state = live.checkpoint();
        change(state);
        rejects([&] { live.restore(state); });
        CHECK(live.world.valid(player));
        CHECK(encode_checkpoint(live.checkpoint()) == before);
    };
    reject([](auto& s) { ++s.content[0]; });
    reject([](auto& s) { s.position.x = NAN; });
    reject([](auto& s) { s.position.x = 1e6F; });
    reject([](auto& s) { s.camera_height = 0; });
    reject([](auto& s) { s.ticks = UINT64_MAX; });
    reject([](auto& s) { s.collected_ids.push_back(UINT64_MAX); });
    CHECK(live.count() > 0);
    reject([](auto& s) { s.collected_ids.push_back(s.collected_ids.front()); });
    reject([](auto& s) { s.door_open = true; });
    reject([](auto& s) { s.won = true; });
    reject([](auto& s) { s.animations[0].clip = "machine"; });
    reject([](auto& s) { s.animations[1].position = UINT64_MAX; });
    // Choose an actual solid cell rather than relying on scene-specific coordinates.
    std::vector<Rect> walls;
    live.map.append_colliders(live.map.bounds(), walls);
    CHECK(!walls.empty());
    reject([&](auto& s) { s.position = (walls.front().min + walls.front().max) * 0.5F; });
}
TEST(checkpoint_decoder_bounds_versions_and_trailing_bytes) {
    const auto bytes = encode_checkpoint(game().checkpoint());
    for (std::size_t n = 0; n < bytes.size(); ++n)
        rejects([&] { decode_checkpoint(std::span(bytes).first(n)); });
    auto bad = bytes;
    bad[4] = 99;
    bad = reseal(bad);
    rejects([&] { decode_checkpoint(bad); });
    bad = bytes;
    bad[56] = 65; // Collected ID count.
    bad = reseal(bad);
    rejects([&] { decode_checkpoint(bad); });
    bad = bytes;
    bad.insert(bad.end() - 4, 0);
    bad = reseal(bad);
    rejects([&] { decode_checkpoint(bad); });
}
TEST(load_missing_or_corrupt_preserves_world) {
    Temporary tmp;
    const auto storage = tmp.storage();
    auto live = game();
    const auto player = live.player;
    const auto before = encode_checkpoint(live.checkpoint());
    rejects([&] { load_slot(storage, 1, live); });
    save_slot(storage, 1, live, false);
    AssetRoot root(storage.paths().state);
    root.write_text("slot-1.esav", "broken");
    rejects([&] { load_slot(storage, 1, live); });
    CHECK(live.world.valid(player) && encode_checkpoint(live.checkpoint()) == before);
}
TEST(interrupted_atomic_writes_keep_old_or_new_complete_record) {
    Temporary tmp;
    AssetRoot root(tmp.path);
    const auto old = encode_config({}),
               replacement = encode_config({{0.2F, 0.3F, 0.4F, true}, false});
    for (const auto stage : {AtomicWriteStage::file_synced, AtomicWriteStage::renamed}) {
        root.write_atomic("settings.ecfg", old);
        const auto child = fork();
        CHECK(child >= 0);
        if (child == 0) {
            static AtomicWriteStage interrupt_at;
            interrupt_at = stage;
            root.write_atomic("settings.ecfg", replacement, [](AtomicWriteStage point) {
                if (point == interrupt_at)
                    _exit(99); // Abrupt process exit, no cleanup.
            });
            _exit(2);
        }
        int status{};
        CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) &&
              WEXITSTATUS(status) == 99);
        const auto bytes = root.read("settings.ecfg", max_record_bytes);
        CHECK(bytes == (stage == AtomicWriteStage::file_synced ? old : replacement));
        decode_config(bytes);
        root.write_atomic("settings.ecfg", replacement); // Orphan temp does not block next save.
        CHECK(root.read("settings.ecfg", max_record_bytes) == replacement);
    }
}
TEST(write_failure_retains_committed_file_and_cleans_temporary) {
    Temporary tmp;
    AssetRoot root(tmp.path);
    const auto bytes = encode_config({});
    root.write_atomic("settings.ecfg", bytes);
    rejects([&] {
        root.write_atomic("settings.ecfg", encode_config({{0, 0, 0, true}, false}),
                          [](AtomicWriteStage stage) {
                              if (stage == AtomicWriteStage::file_synced)
                                  throw std::runtime_error("Injected failure before rename");
                          });
    });
    CHECK(root.read("settings.ecfg", max_record_bytes) == bytes);
    CHECK(std::distance(std::filesystem::directory_iterator(tmp.path),
                        std::filesystem::directory_iterator{}) == 1);
    std::filesystem::create_directory(tmp.path / "blocked");
    rejects([&] { root.write_atomic("blocked", bytes); });
}
TEST(menu_checkpoint_preserves_underlying_pause_state) {
    Temporary tmp;
    auto live = game();
    Settings settings;
    InputFrame open{};
    open[static_cast<std::size_t>(Key::settings)].pressed = true;
    settings.update(live, open, 1280, 720, false);
    CHECK(live.paused && !settings.gameplay_paused(live));
    save_slot(tmp.storage(), 1, live, settings.gameplay_paused(live));
    load_slot(tmp.storage(), 1, live);
    settings.loaded(live);
    CHECK(live.paused && settings.opened() && !settings.gameplay_paused(live));
    settings.update(live, open, 1280, 720, false);
    CHECK(!live.paused && !settings.opened());
    live.paused = true;
    settings.update(live, open, 1280, 720, false);
    save_slot(tmp.storage(), 2, live, settings.gameplay_paused(live));
    load_slot(tmp.storage(), 2, live);
    settings.loaded(live);
    settings.update(live, open, 1280, 720, false);
    CHECK(live.paused);
}
} // namespace
int main() {
    return testing::run_tests();
}
