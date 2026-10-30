#include "../examples/feature_lab/scene_flow.hpp"
#include "../examples/feature_lab/soundtrack.hpp"
#include "../examples/feature_lab/texture_assets.hpp"
#include "engine/window.hpp"
#include "scene_fixture.hpp"
#include <chrono>
#include <thread>

namespace {
using namespace engine;
using namespace feature_lab;
using namespace testing;
using namespace std::chrono_literals;
constexpr unsigned cycles = 64, max_steps = 3000;
template <class F> void eventually(F condition) {
    const auto deadline = std::chrono::steady_clock::now() + 3s;
    while (!condition()) {
        CHECK(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(1ms);
    }
}
InputFrame press(Key key) {
    InputFrame input{};
    input[static_cast<std::size_t>(key)].pressed = true;
    return input;
}
TEST(repeated_complete_routes_reload_rollback_saves_and_audio_retirement) {
    SceneFiles files; // Private assets and user data; never writes into the installation.
    const UserStorage storage(UserPaths::discover(files.path / "user"));
    Window window{64, 64, "Bounded session regression", false};
    Renderer renderer;
    const auto empty = renderer.memory();
    auto game = load_game(files.root);
    TextureAssets textures(files.root);
    textures.load(renderer, game.map.data().texture, 6, 1);
    textures.load(renderer, game.animations->texture, game.animations->atlas_columns,
                  game.animations->atlas_rows);
    game.world.each([&](const SceneNode& node) {
        if (node.sprite)
            textures.load(renderer, node.sprite->texture, 1, 1);
    });
    const auto target = renderer.create_target(64, 64);
    const auto resident = renderer.memory();
    const auto texture_count = renderer.live_textures();
    const auto collision_bytes = game.collisions.buffer_bytes();
    SceneAssets scenes;
    SceneFlow flow(game, false, &files.root);
    flow.prepare_replacement = [&](const Game& candidate) { textures.validate_level(candidate); };
    std::unique_ptr<Soundtrack> audio;
    std::array<std::size_t, 2> completed_scene_bytes{};
    std::uint64_t ticks = 0, draws = 0;
    unsigned outputs = 0;
    const auto started = std::chrono::steady_clock::now();
    const auto render = [&] {
        const auto bounds = game.map.bounds();
        renderer.begin(target, {(bounds.min + bounds.max) * .5F, 80});
        for (const auto entity : game.draw_order) {
            if (!game.world.valid(entity))
                continue;
            const auto& node = game.world.get(entity);
            const auto& sprite = *node.sprite;
            renderer.sprite(textures.at(sprite.texture).gpu,
                            game.render_transform(entity, 1) * Transform::from({}, 0, sprite.size));
        }
        renderer.end();
        CHECK(renderer.stats().quads > 0 && renderer.healthy());
        CHECK(renderer.memory() == resident);
        ++draws;
    };
    for (unsigned cycle = 0; cycle < cycles; ++cycle) {
        CHECK(game.level == (cycle % 2 ? Level::annex : Level::facility));
        if (cycle % 8 == 0) {
            audio.reset(); // Join the previous real worker before opening another null sink.
            audio = std::make_unique<Soundtrack>(files.root, true, "null");
            ++outputs;
        } else {
            audio->update(game, press(Key::restart), 0);
        }
        CHECK(audio->enabled());
        audio->locate_emitter(game);
        eventually([&] { return audio->stats().active_voices == 2; });
        const auto sound_bytes = audio->sound_bytes();
        Replay route;
        unsigned steps = 0;
        for (; steps < max_steps && !game.won; ++steps) {
            if (steps == 120) {
                const auto checkpoint = encode_checkpoint(game.checkpoint());
                save_slot(storage, 1, game, game.paused);
                game.update(route.next(game)); // Prove restore replaces a changed live state.
                CHECK(encode_checkpoint(game.checkpoint()) != checkpoint);
                load_slot(storage, 1, game, &files.root, flow.prepare_replacement);
                CHECK(encode_checkpoint(game.checkpoint()) == checkpoint);
                audio->restored(game);
                auto corrupt = checkpoint;
                corrupt.back() ^= 1;
                // UserStorage intentionally rejects corrupt records on write. Inject external
                // corruption below that API so this exercises load rollback.
                AssetRoot(storage.paths().state).write_atomic("slot-1.esav", corrupt);
                rejects([&] { load_slot(storage, 1, game, &files.root); });
                CHECK(encode_checkpoint(game.checkpoint()) == checkpoint);
                storage.write_slot(1, checkpoint);

                const auto original = files.text(game.level);
                const auto player = game.player;
                const auto scene_revision = scenes.revision();
                files.write("invalid scene", game.level);
                CHECK(!scenes.reload(game, files.root));
                CHECK(scenes.revision() == scene_revision && game.world.valid(player));
                CHECK(encode_checkpoint(game.checkpoint()) == checkpoint);
                files.write(original, game.level);
                CHECK(scenes.reload(game, files.root, flow.prepare_replacement));
                CHECK(scenes.revision() == scene_revision + 1 && !game.world.valid(player));
                CHECK(encode_checkpoint(game.checkpoint()) == checkpoint);

                const auto texture_revision = textures.revision();
                const StagingScope staging;
                CHECK(!textures.reload(renderer, true));
                CHECK(textures.revision() == texture_revision && renderer.memory() == resident);
                CHECK(staging.snapshot().clean());
                CHECK(textures.reload(renderer));
                CHECK(textures.revision() == texture_revision + 1);
                CHECK(staging.snapshot().clean() && staging.snapshot().combined_peak > 0);

                const auto wav = files.root.read("sounds/door.wav", SoundData::max_file_bytes);
                const auto audio_revision = audio->revision();
                files.root.write_text("sounds/door.wav", "invalid WAV");
                CHECK(!audio->reload(files.root) && audio->revision() == audio_revision);
                CHECK(audio->sound_bytes() == sound_bytes && audio->enabled());
                files.root.write_atomic("sounds/door.wav", wav);
                CHECK(audio->reload(files.root) && audio->revision() == audio_revision + 1);
                eventually([&] {
                    audio->collect();
                    return audio->retained_sound_bytes() == sound_bytes;
                });
                const auto pause_ticks = game.ticks;
                const auto pause = press(Key::pause);
                game.update(pause);
                audio->update(game, pause, 0);
                game.update({});
                CHECK(game.paused && game.ticks == pause_ticks);
                game.update(pause);
                audio->update(game, pause, 0);
                CHECK(!game.paused);
            }
            const auto controls = route.next(game);
            const auto picked = game.update(controls);
            audio->update(game, controls, picked);
            CHECK(audio->enabled());
            if (steps % 120 == 0)
                render();
            if (steps % 16 == 0)
                std::this_thread::sleep_for(1ms); // Cooperate with the real audio worker.
        }
        CHECK(steps > 120 && game.won && game.count() == 3 && game.door_completions == 1);
        ticks += game.ticks;
        eventually([&] {
            audio->collect();
            return audio->stats().active_voices == 1 &&
                   audio->retained_sound_bytes() == sound_bytes;
        });
        const auto stats = audio->stats();
        CHECK(!stats.queue_rejections && !stats.dropped_plays && !stats.stale_controls &&
              !stats.device_error);
        CHECK(game.collisions.buffer_bytes() == collision_bytes);
        const auto index = cycle % 2;
        const auto scene_bytes = game.world.buffer_bytes();
        if (cycle < 2)
            completed_scene_bytes[index] = scene_bytes;
        CHECK(scene_bytes == completed_scene_bytes[index]);
        const auto transient = renderer.create_target(16, 16);
        renderer.resize_target(transient, 32, 16);
        renderer.release(transient);
        render();
        CHECK(renderer.live_textures() == texture_count && renderer.live_targets() == 1);
        std::cout << "[cycle] completed=" << cycle + 1 << " ticks=" << game.ticks
                  << " scene_bytes=" << scene_bytes << " sound_bytes=" << sound_bytes << '\n';
        flow.advance_level(game, {});
    }
    audio.reset();
    for (const auto& key : textures.keys())
        renderer.release(textures.at(key).gpu);
    renderer.release(target);
    CHECK(renderer.live_textures() == 0 && renderer.live_targets() == 0);
    CHECK(renderer.memory() == empty && renderer.healthy());
    const auto elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::cout << "[session] cycles=" << cycles << " max_steps_per_cycle=" << max_steps
              << " ticks=" << ticks << " renders=" << draws << " audio_outputs=" << outputs
              << " elapsed_seconds=" << elapsed << " resources_restored=true\n";
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
