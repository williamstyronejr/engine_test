#include "../examples/feature_lab/scene_flow.hpp"
#include "test.hpp"
#include <cstdlib>
#include <filesystem>
namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
struct Files {
    std::filesystem::path path;
    Files() {
        auto pattern = (std::filesystem::temp_directory_path() / "flow-XXXXXX").string();
        if (!mkdtemp(pattern.data()))
            throw std::runtime_error("Cannot create fixture");
        path = pattern;
    }
    ~Files() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    UserStorage storage() const { return UserStorage({path / "config", path / "state"}); }
};
struct Fixture {
    Game game = load_game(AssetRoot(TEST_ASSET_ROOT));
    SceneFlow flow{game, true};
    Files files;
    UserStorage storage = files.storage();
    FlowStep update(InputFrame input = {}) { return flow.update(game, input, 640, 650, &storage); }
    FlowStep click(UiId id) {
        update();
        for (const auto& widget : flow.ui().widgets()) {
            if (widget.id != id)
                continue;
            const auto center = (widget.bounds.min + widget.bounds.max) * 0.5F;
            Input pointer;
            pointer.move_pointer(center);
            pointer.set_primary(true);
            update(pointer.consume());
            pointer.set_primary(false);
            return update(pointer.consume());
        }
        throw std::runtime_error("Missing widget");
    }
};
TEST(title_new_game_resume_and_paused_session) {
    Fixture f;
    CHECK(f.flow.title_open() && !f.flow.has_session() && f.game.paused);
    CHECK(!f.click(SceneFlow::resume).transitioned);
    const auto old = f.game.player;
    const auto start = f.click(SceneFlow::new_game);
    CHECK(start.transitioned && start.replaced && !f.flow.title_open() && !f.game.paused);
    CHECK(!f.game.world.valid(old) && f.game.ticks == 0);
    f.game.update({});
    const auto player = f.game.player;
    const auto bytes = encode_checkpoint(f.game.checkpoint());
    f.flow.enter_title(f.game);
    CHECK(f.game.paused && !f.flow.gameplay_paused(f.game));
    const auto resumed = f.click(SceneFlow::resume);
    CHECK(resumed.transitioned && !resumed.replaced && f.game.world.valid(player));
    CHECK(encode_checkpoint(f.game.checkpoint()) == bytes);
    f.game.paused = true;
    f.flow.enter_title(f.game);
    f.click(SceneFlow::resume);
    CHECK(f.game.paused);
}
TEST(failed_continue_retains_session_and_success_restores_checkpoint) {
    Fixture f;
    f.flow.start_new(f.game);
    for (int i = 0; i < 12; ++i)
        f.game.update({});
    const auto saved = f.game.checkpoint();
    save_slot(f.storage, 2, f.game, false);
    f.flow.enter_title(f.game);
    const auto before = encode_checkpoint(f.game.checkpoint());
    const auto player = f.game.player;
    CHECK(!f.click(SceneFlow::continue_game).transitioned); // Empty slot 1.
    CHECK(f.flow.title_open() && f.game.world.valid(player));
    CHECK(encode_checkpoint(f.game.checkpoint()) == before && !f.flow.notice.empty());
    AssetRoot(f.storage.paths().state).write_text("slot-1.esav", "corrupt");
    CHECK(!f.click(SceneFlow::continue_game).transitioned);
    CHECK(encode_checkpoint(f.game.checkpoint()) == before);
    f.click(SceneFlow::slot);
    CHECK(f.flow.selected_slot == 2);
    CHECK(f.click(SceneFlow::continue_game).replaced);
    CHECK(!f.flow.title_open() && !f.game.world.valid(player));
    CHECK(encode_checkpoint(f.game.checkpoint()) == encode_checkpoint(saved));
}
TEST(new_game_and_invalid_checkpoint_are_transactional) {
    Fixture f;
    f.flow.start_new(f.game);
    f.game.update({});
    f.flow.enter_title(f.game);
    const auto before = encode_checkpoint(f.game.checkpoint());
    auto invalid = f.game.checkpoint();
    invalid.position.x = INFINITY;
    f.storage.write_slot(1, encode_checkpoint(invalid));
    CHECK(!f.click(SceneFlow::continue_game).transitioned);
    CHECK(encode_checkpoint(f.game.checkpoint()) == before);
    f.game.world.each([&](const SceneNode&) { rejects([&] { f.flow.start_new(f.game); }); });
    CHECK(f.flow.title_open() && encode_checkpoint(f.game.checkpoint()) == before);
    CHECK(f.click(SceneFlow::new_game).transitioned);
    CHECK(f.game.ticks == 0 && f.game.count() == 0 && !f.game.paused);
}
TEST(title_input_is_captured_until_release_and_quit_is_explicit) {
    Fixture f;
    Input input;
    input.set(Key::left, true);
    input.set(Key::accept, true); // Default focus is New Game.
    const auto start = f.update(input.consume());
    CHECK(start.transitioned && !button(start.gameplay, Key::left).held);
    CHECK(!button(f.update(input.consume()).gameplay, Key::left).held);
    input.set(Key::left, false);
    f.update(input.consume());
    input.set(Key::left, true);
    CHECK(button(f.update(input.consume()).gameplay, Key::left).pressed);
    f.flow.enter_title(f.game);
    InputFrame escape{};
    escape[static_cast<std::size_t>(Key::escape)].pressed = true;
    CHECK(f.update(escape).quit && f.flow.title_open());
}
TEST(title_layout_first_frame_resize_and_missing_storage) {
    Fixture f;
    for (const auto size : {std::pair{320, 240}, {640, 480}, {1280, 720}, {1050, 1360}}) {
        f.flow.prepare(size.first, size.second, false);
        CHECK(f.flow.ui().widgets().size() == 5);
        for (const auto& widget : f.flow.ui().widgets()) {
            CHECK(widget.bounds.min.x >= 0 && widget.bounds.min.y >= 0);
            CHECK(widget.bounds.max.x <= static_cast<float>(size.first));
            CHECK(widget.bounds.max.y <= static_cast<float>(size.second));
            if (widget.id == SceneFlow::continue_game || widget.id == SceneFlow::resume)
                CHECK(!widget.enabled);
        }
    }
    const auto state = encode_checkpoint(f.game.checkpoint());
    rejects([&] { f.flow.continue_slot(f.game, nullptr); });
    CHECK(f.flow.title_open() && encode_checkpoint(f.game.checkpoint()) == state);
}
TEST(settings_return_to_title_restores_underlying_pause) {
    Fixture f;
    f.flow.start_new(f.game);
    Settings settings;
    InputFrame open{};
    open[static_cast<std::size_t>(Key::settings)].pressed = true;
    settings.update(f.game, open, 640, 480, false);
    for (const auto& widget : settings.ui().widgets()) {
        if (widget.id != Settings::title)
            continue;
        Input input;
        input.move_pointer((widget.bounds.min + widget.bounds.max) * 0.5F);
        input.set_primary(true);
        settings.update(f.game, input.consume(), 640, 480, false);
        input.set_primary(false);
        const auto result = settings.update(f.game, input.consume(), 640, 480, false);
        CHECK(result.title && result.captured && !settings.opened() && !f.game.paused);
        f.flow.enter_title(f.game);
        CHECK(!f.flow.gameplay_paused(f.game));
        return;
    }
    CHECK(false);
}
} // namespace
int main() {
    return testing::run_tests();
}
