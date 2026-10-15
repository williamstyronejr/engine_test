#include "animation_fixture.hpp"
#include <limits>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
std::shared_ptr<const AnimationSet> bank(AnimationSet set) {
    return std::make_shared<const AnimationSet>(std::move(set));
}
TEST(player_replacement_preserves_phase_pause_name_and_old_snapshot) {
    AnimationSet source{
        "a.etex", 4, 1, {{"loop", true, {{0, 2}, {1, 4}}}, {"once", false, {{2, 6}}}}};
    const auto original = bank(source);
    AnimationPlayer player(original), old(original);
    player.advance(3);
    player.pause(true);
    std::reverse(source.clips.begin(), source.clips.end());
    clip(source, "loop").frames = {{2, 4}, {3, 6}};
    player.replace(bank(source));
    CHECK(player.clip_name() == "loop" && player.position() == 5 && player.paused());
    CHECK(player.frame_index() == 1 && player.uv().min.x == .75F);
    CHECK(!player.advance(100).completed && player.position() == 5);
    CHECK(old.position() == 0 && old.uv().min.x == 0);
    const auto retained = player.snapshot();
    rejects([&] { player.replace(nullptr); });
    clip(source, "loop").loop = false;
    rejects([&] { player.replace(bank(source)); });
    source.clips.pop_back(); // Removes active loop after reversal.
    rejects([&] { player.replace(bank(source)); });
    CHECK(player.snapshot() == retained);
}
TEST(oneshot_replacement_preserves_completion_without_emitting_an_extra_event) {
    AnimationSet set{"a.etex", 2, 1, {{"once", false, {{0, 100}}}}};
    AnimationPlayer player(bank(set));
    player.advance(99);
    set.clips[0].frames = {{1, 1}};
    player.replace(bank(set));
    CHECK(player.position() == 0 && !player.finished());
    CHECK(player.advance(1).completed && player.finished());
    set.clips[0].frames = {{0, 17}, {1, 13}};
    player.pause(true);
    player.replace(bank(set));
    CHECK(player.position() == 30 && player.finished() && player.paused());
    player.pause(false);
    CHECK(!player.advance(std::numeric_limits<std::uint64_t>::max()).completed);
}
TEST(maximum_duration_replacement_uses_exact_integer_progress) {
    AnimationSet set{
        "a.etex", 1, 1, {{"loop", true, std::vector<AnimationFrame>(256, {0, 36000})}}};
    const std::uint64_t duration = 256 * 36000;
    AnimationPlayer player(bank(set));
    player.advance(duration - 1);
    player.replace(bank(set));
    CHECK(player.position() == duration - 1);
    set.clips[0].frames = {{0, 1}};
    player.replace(bank(set));
    CHECK(player.position() == 0 && player.advance(10).loops == 10);
}
TEST(game_bank_swap_preserves_state_and_save_content_identity_rules) {
    AnimationFiles f;
    auto game = load_game(f.root);
    for (int i = 0; i < 9; ++i)
        game.update({});
    game.paused = true;
    const auto before = game.checkpoint();
    auto candidate = *game.animations;
    for (auto& c : candidate.clips)
        for (auto& frame : c.frames)
            frame.ticks *= 2;
    const auto previous_bank = game.animations;
    f.write(candidate);
    AnimationAssets reload;
    CHECK(reload.reload(game, f.root) && reload.revision() == 2);
    const auto after = game.checkpoint();
    CHECK(after.ticks == before.ticks && after.paused == before.paused);
    CHECK(game.position.x == before.position.x && game.position.y == before.position.y);
    CHECK(after.content[0] == before.content[0] && after.content[1] == before.content[1]);
    CHECK(after.content[2] != before.content[2]);
    for (std::size_t i = 0; i < before.animations.size(); ++i) {
        CHECK(after.animations[i].position == before.animations[i].position * 2);
        CHECK(after.animations[i].paused == before.animations[i].paused);
    }
    rejects([&] { game.restore(before); });
    game.restore(after);
    auto fresh = load_game(f.root);
    fresh.restore(after);
    CHECK(fresh.checkpoint().animations == after.animations);
    CHECK(previous_bank->clips[0].frames[0].ticks * 2 == game.animations->clips[0].frames[0].ticks);
    reload.replaced();
    CHECK(reload.revision() == 1 && reload.applied() && reload.diagnostic().empty());
}
TEST(required_future_clips_atlas_and_loop_modes_are_transactional) {
    AnimationFiles f;
    auto game = load_game(f.root);
    const auto old = game.animations;
    const auto before = game.checkpoint();
    AnimationAssets reload;
    for (int i = 0; i < 5; ++i) {
        auto bad = *old;
        if (i == 0)
            bad.clips.erase(bad.clips.begin() + 1); // Idle now, walk needed later.
        if (i == 1)
            bad.texture = "textures/tiles.etex";
        if (i == 2)
            bad.atlas_columns = 16;
        if (i == 3)
            clip(bad, "door_open").loop = true;
        if (i == 4)
            clip(bad, "machine").loop = false;
        f.write(bad);
        CHECK(!reload.reload(game, f.root));
        CHECK(game.animations == old && game.checkpoint().animations == before.animations);
        CHECK(game.content_id() == before.content && reload.revision() == 1);
        CHECK(reload.diagnostic().find("facility.eani") != std::string::npos);
    }
}
TEST(file_failures_repair_locked_sessions_and_snapshot_release) {
    AnimationFiles f;
    auto game = load_game(f.root);
    const auto good = *game.animations;
    AnimationAssets reload;
    for (int mode = 0; mode < 3; ++mode) {
        if (mode == 0)
            f.root.write_text(AnimationAssets::key, "invalid EANI");
        if (mode == 1)
            std::filesystem::remove(f.path / AnimationAssets::key);
        if (mode == 2)
            std::filesystem::create_symlink("/etc/passwd", f.path / AnimationAssets::key);
        CHECK(!reload.reload(game, f.root) && reload.revision() == 1);
    }
    std::filesystem::remove(f.path / AnimationAssets::key);
    f.write(good);
    for (int i = 0; i < 20; ++i) {
        std::weak_ptr<const AnimationSet> old = game.animations;
        CHECK(reload.reload(game, f.root));
        CHECK(old.expired());
    }
    CHECK(reload.revision() == 21 && reload.diagnostic().empty());
    AnimationAssets locked(true);
    const auto old = game.animations;
    f.root.write_text(AnimationAssets::key, "invalid EANI");
    CHECK(!locked.reload(game, f.root) && locked.locked());
    CHECK(locked.applied() && locked.revision() == 1 && game.animations == old);
}
TEST(mid_door_and_finished_game_reloads_preserve_exactly_once_completion) {
    AnimationFiles f;
    auto game = load_game(f.root);
    Replay route;
    while (!game.door_started) {
        CHECK(game.ticks < 2400);
        game.update(route.next(game));
    }
    auto edited = *game.animations;
    clip(edited, "door_open").frames = {{19, 1}};
    game.reload_animations(bank(edited));
    CHECK(!game.door_open && game.door_completions == 0);
    game.update(route.next(game));
    CHECK(game.door_open && game.door_completions == 1);
    while (!game.won) {
        CHECK(game.ticks < 2400);
        game.update(route.next(game));
    }
    clip(edited, "door_open").frames = {{16, 30}, {19, 30}};
    game.reload_animations(bank(edited));
    CHECK(game.won && game.door_open && game.door_animation.finished());
    game.restore(game.checkpoint());
    CHECK(game.door_completions == 1);
    CHECK(!game.door_animation.advance(100).completed);
}
} // namespace
int main() {
    return testing::run_tests();
}
