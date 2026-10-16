#include "../examples/feature_lab/soundtrack.hpp"
#include "test.hpp"
#include <alsa/asoundlib.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>
#include <unistd.h>

namespace {
std::atomic<bool> hold_worker{}, worker_held{};
}
extern "C" snd_pcm_sframes_t __real_snd_pcm_writei(snd_pcm_t*, const void*, snd_pcm_uframes_t);
extern "C" snd_pcm_sframes_t __wrap_snd_pcm_writei(snd_pcm_t* pcm, const void* data,
                                                   snd_pcm_uframes_t frames) {
    if (hold_worker.load()) {
        worker_held.store(true);
        while (hold_worker.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return __real_snd_pcm_writei(pcm, data, frames);
}
namespace {
using namespace engine;
using namespace testing;
template <class F> void eventually(F condition) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!condition()) {
        CHECK(std::chrono::steady_clock::now() < deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
struct Hold {
    Hold() {
        worker_held = false;
        hold_worker = true;
    }
    void wait() {
        eventually([] { return worker_held.load(); });
    }
    ~Hold() { hold_worker = false; }
};
TEST(queued_chained_banks_retain_storage_until_ack_and_keep_voice_tickets) {
    AudioOutput audio("null");
    const auto a = audio.register_sound({1, {.2F, .4F}});
    const auto b = audio.register_sound({1, {.3F}});
    CHECK(audio.pause_group(AudioGroup::effects, true));
    const auto voice = audio.play(a, {1, 0, true});
    CHECK(voice);
    eventually([&] { return audio.stats().processed == 2; });
    {
        Hold hold;
        hold.wait();
        std::array batch{SoundReplacement{a, {2, {.5F, .6F}}},
                         SoundReplacement{b, {1, {.7F, .8F}}}};
        CHECK(audio.replace_sounds(batch));
        CHECK(audio.sound_bytes() == 16 && audio.retained_sound_bytes() == 28);
        std::array again{SoundReplacement{a, {1, {.9F}}}};
        CHECK(audio.replace_sounds(again));
        CHECK(audio.sound_bytes() == 12 && audio.retained_sound_bytes() == 32);
        audio.collect_sounds();
        CHECK(audio.retained_sound_bytes() == 32);
        CHECK(audio.set_pan(voice, .2F));
        CHECK(audio.play(a, {1, 0, true}));
        CHECK(audio.play(b, {1, 0, true}));
    }
    eventually([&] { return audio.stats().processed == 7; });
    audio.collect_sounds();
    CHECK(audio.sound_bytes() == 12 && audio.retained_sound_bytes() == 12);
    CHECK(audio.sound_count() == 2 && audio.stats().started == 3 &&
          audio.stats().active_voices == 3);
    CHECK(audio.stats().stale_controls == 0 && audio.stats().dropped_plays == 0);
    for (int i = 0; i < 100; ++i) {
        std::array batch{SoundReplacement{a, {1, {.2F}}}};
        CHECK(audio.replace_sounds(batch));
        eventually([&] { return audio.stats().processed == static_cast<std::uint64_t>(8 + i); });
        audio.collect_sounds();
        CHECK(audio.retained_sound_bytes() == 12 && audio.sound_count() == 2);
    }
}
TEST(queue_rejection_and_invalid_batches_leave_bank_and_existing_voice_intact) {
    AudioOutput audio("null");
    const auto h = audio.register_sound({1, {.2F}});
    SoundHandle other;
    {
        AudioOutput foreign("null");
        other = foreign.register_sound({1, {.3F}});
    }
    CHECK(audio.play(h, {1, 0, true}));
    eventually([&] { return audio.stats().processed == 1; });
    for (int mode = 0; mode < 4; ++mode) {
        std::vector<SoundReplacement> batch{{h, {1, {.5F, .6F}}}};
        if (mode == 0)
            batch[0].handle = other;
        if (mode == 1)
            batch.push_back({h, {1, {.4F}}});
        if (mode == 2)
            batch[0].data.samples[0] = NAN;
        if (mode == 3)
            batch.clear();
        rejects([&] { audio.replace_sounds(batch); });
        CHECK(audio.sound_bytes() == 4 && audio.retained_sound_bytes() == 4);
    }
    {
        Hold hold;
        hold.wait();
        for (int i = 0; i < 63; ++i)
            CHECK(audio.set_master_gain(1));
        std::array batch{SoundReplacement{h, {1, {.8F, .9F}}}};
        CHECK(!audio.replace_sounds(batch));
        CHECK(audio.sound_bytes() == 4 && audio.retained_sound_bytes() == 4);
    }
    eventually([&] { return audio.stats().processed == 64; });
    CHECK(audio.stats().active_voices == 1 && audio.stats().started == 1 && audio.healthy());
}
TEST(retained_memory_budget_is_checked_before_publication) {
    AudioOutput audio("null");
    std::vector<SoundHandle> handles;
    for (int i = 0; i < 5; ++i)
        handles.push_back(audio.register_sound({1, std::vector<float>(SoundData::max_frames, 0)}));
    const auto bytes = audio.sound_bytes();
    std::array batch{
        SoundReplacement{handles[0], {1, std::vector<float>(SoundData::max_frames, .1F)}}};
    rejects([&] { audio.replace_sounds(batch); });
    CHECK(audio.sound_bytes() == bytes && audio.retained_sound_bytes() == bytes);
}
struct Files {
    static std::filesystem::path create() {
        auto name = (std::filesystem::temp_directory_path() / "audio-reload-XXXXXX").string();
        CHECK(mkdtemp(name.data()));
        return name;
    }
    std::filesystem::path path{create()};
    AssetRoot root{path};
    Files() {
        std::filesystem::copy(TEST_ASSET_ROOT, path,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
    }
    ~Files() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
TEST(soundtrack_failures_repair_pause_settings_and_completion_are_preserved) {
    Files f;
    auto game = feature_lab::load_game(f.root);
    feature_lab::Soundtrack track(f.root, true, "null", {.4F, .2F, .3F, true}, true);
    CHECK(track.enabled());
    eventually([&] { return track.stats().started == 2; });
    const auto original = f.root.read("sounds/door.wav", SoundData::max_file_bytes);
    const auto bytes = track.sound_bytes();
    for (int mode = 0; mode < 3; ++mode) {
        if (mode == 0)
            f.root.write_text("sounds/door.wav", "invalid WAV");
        if (mode == 1)
            std::filesystem::remove(f.path / "sounds/door.wav");
        if (mode == 2)
            std::filesystem::create_symlink("/etc/passwd", f.path / "sounds/door.wav");
        CHECK(!track.reload(f.root) && track.enabled() && track.revision() == 1);
        CHECK(track.sound_bytes() == bytes &&
              track.reload_diagnostic().find("door.wav") != std::string::npos);
        CHECK(track.settings().muted && track.stats().started == 2);
    }
    std::filesystem::remove(f.path / "sounds/door.wav");
    f.root.write_atomic("sounds/door.wav", original);
    const auto processed = track.stats().processed;
    CHECK(track.reload(f.root) && track.revision() == 2 && track.reload_diagnostic().empty());
    eventually([&] { return track.stats().processed == processed + 1; });
    track.collect();
    CHECK(track.retained_sound_bytes() == bytes);
    CHECK(track.stats().started == 2 && track.stats().completed == 0 && track.settings().muted);
    // A completed game has stopped its machine loop. Reload must not resurrect it.
    game.won = true;
    game.paused = false;
    track.update(game, {}, 0);
    eventually([&] { return track.stats().active_voices == 1; });
    CHECK(track.reload(f.root));
    eventually([&] {
        track.collect();
        return track.retained_sound_bytes() == track.sound_bytes();
    });
    CHECK(track.stats().started == 2 && track.stats().active_voices == 1);
    feature_lab::Soundtrack off(f.root, false);
    CHECK(!off.reload(f.root) && off.revision() == 1);
}
TEST(device_failure_rejects_reload_and_shutdown_safely_releases_pending_banks) {
    {
        AudioOutput audio("file:/dev/full");
        auto h = audio.register_sound({1, {.2F}});
        eventually([&] { return !audio.healthy(); });
        std::array batch{SoundReplacement{h, {1, {.7F}}}};
        CHECK(!audio.replace_sounds(batch) && audio.sound_bytes() == 4);
    }
    for (int i = 0; i < 10; ++i) {
        AudioOutput audio("null");
        auto h = audio.register_sound({1, {.2F}});
        Hold hold;
        hold.wait();
        CHECK(audio.play(h, {1, 0, true}));
        std::array batch{SoundReplacement{h, {1, {.7F}}}};
        CHECK(audio.replace_sounds(
            batch)); // Guard releases worker; immediate output destruction joins it.
    }
}
} // namespace
int main() {
    return testing::run_tests();
}
