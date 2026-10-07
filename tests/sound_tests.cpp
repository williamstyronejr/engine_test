#include "../engine/src/audio_queue.hpp"
#include "engine/assets.hpp"
#include "engine/mixer.hpp"
#include "engine/sound.hpp"
#include "test.hpp"
#include <limits>

namespace {
using namespace engine;
using namespace testing;
void put(std::vector<std::uint8_t>& b, std::size_t p, std::uint32_t v, unsigned n = 4) {
    for (unsigned i = 0; i < n; ++i)
        b.at(p + i) = static_cast<std::uint8_t>(v >> (8 * i));
}
std::vector<std::uint8_t> fixture(std::uint32_t rate = 48000) {
    // Independent mono fixture: -32768, 0, 16384, 32767.
    std::vector<std::uint8_t> b = {'R', 'I', 'F', 'F', 44, 0, 0,   0, 'W', 'A', 'V', 'E', 'f',
                                   'm', 't', ' ', 16,  0,  0, 0,   1, 0,   1,   0,   0,   0,
                                   0,   0,   0,   0,   0,  0, 2,   0, 16,  0,   'd', 'a', 't',
                                   'a', 8,   0,   0,   0,  0, 128, 0, 0,   0,   64,  255, 127};
    put(b, 24, rate);
    put(b, 28, rate * 2);
    return b;
}
TEST(initial_mixer_preferences_apply_from_first_sample) {
    const std::array<float, 2> samples{1, 1};
    std::array<float, 4> output{};
    Mixer muted({0, 1, 1});
    CHECK(muted.play(SoundView{samples, 1}, {1, 0, true, AudioGroup::music}));
    muted.mix(output);
    for (auto sample : output)
        CHECK(sample == 0);
    Mixer quiet({0.5F, 0.2F, 0.4F});
    CHECK(quiet.play(SoundView{samples, 1}, {1, 0, true, AudioGroup::music}));
    quiet.mix(output);
    for (auto sample : output)
        NEAR(sample, 0.1F);
    rejects([] { Mixer bad({NAN, 1, 1}); });
    rejects([] { Mixer bad({1, -1, 1}); });
}
TEST(wav_pcm16_and_stereo_roundtrip) {
    const auto sound = decode_wav(fixture());
    CHECK(sound.channels == 1 && sound.samples.size() == 4);
    NEAR(sound.samples[0], -1.0F);
    NEAR(sound.samples[1], 0.0F);
    NEAR(sound.samples[2], 0.5F);
    NEAR(sound.samples[3], 32767.0F / 32768);
    CHECK(encode_wav(sound) == fixture());
    SoundData stereo{2, {-1, 0.5F, 0, -0.25F, 0.75F, 0}};
    CHECK(decode_wav(encode_wav(stereo)).samples == stereo.samples);
    stereo.samples[0] = 1;
    NEAR(decode_wav(encode_wav(stereo)).samples[0], 32767.0F / 32768);
}
TEST(wav_chunk_padding_unknown_chunks_and_order) {
    auto b = fixture();
    b.insert(b.begin() + 12, {'J', 'U', 'N', 'K', 1, 0, 0, 0, 55, 0});
    put(b, 4, static_cast<std::uint32_t>(b.size() - 8));
    CHECK(decode_wav(b).samples == decode_wav(fixture()).samples);
    b.erase(b.begin() + 21); // Missing odd-chunk padding makes following chunks invalid.
    put(b, 4, static_cast<std::uint32_t>(b.size() - 8));
    rejects([&] { decode_wav(b); });
    b = fixture();
    std::rotate(b.begin() + 12, b.begin() + 36, b.end());
    CHECK(decode_wav(b).samples == decode_wav(fixture()).samples);
    b = fixture();
    b.insert(b.begin() + 36, {0, 0});
    put(b, 16, 18);
    put(b, 4, 46);
    CHECK(decode_wav(b).samples.size() == 4);
    b[36] = 1;
    rejects([&] { decode_wav(b); });
}
TEST(wav_rejects_truncation_corruption_and_resource_limits) {
    const auto b = fixture();
    for (std::size_t n = 0; n < b.size(); ++n)
        rejects([&] { decode_wav(std::span(b).first(n)); });
    for (const auto offset : {0U, 4U, 8U, 16U, 20U, 22U, 24U, 28U, 32U, 34U, 40U}) {
        auto bad = b;
        put(bad, offset, 0xffffffffU,
            offset == 20 || offset == 22 || offset == 32 || offset == 34 ? 2 : 4);
        rejects([&] { decode_wav(bad, "bad.wav"); });
    }
    auto bad = b;
    bad.push_back(0);
    rejects([&] { decode_wav(bad); });
    bad = b;
    bad.insert(bad.end(), b.begin() + 36, b.end());
    put(bad, 4, static_cast<std::uint32_t>(bad.size() - 8));
    rejects([&] { decode_wav(bad); }); // Duplicate data.
    bad = b;
    bad.insert(bad.end(), b.begin() + 12, b.begin() + 36);
    put(bad, 4, static_cast<std::uint32_t>(bad.size() - 8));
    rejects([&] { decode_wav(bad); }); // Duplicate fmt.
    bad.resize(SoundData::max_file_bytes + 1);
    rejects([&] { decode_wav(bad); });
    bad = b;
    bad.resize(44 + (SoundData::max_frames + 1) * 2);
    put(bad, 4, static_cast<std::uint32_t>(bad.size() - 8));
    put(bad, 40, static_cast<std::uint32_t>(bad.size() - 44));
    rejects([&] { decode_wav(bad); }); // Duration checked before sample allocation.
    rejects([] { encode_wav({1, {std::numeric_limits<float>::quiet_NaN()}}); });
    rejects([] { encode_wav({2, {0}}); });
    rejects([] { encode_wav({1, {1.1F}}); });
}
TEST(wav_resampling_uses_rational_positions_and_preserves_channels) {
    const auto source = decode_wav(fixture());
    const auto doubled = decode_wav(fixture(24000));
    CHECK(doubled.samples.size() == 8);
    NEAR(doubled.samples[0], -1.0F);
    NEAR(doubled.samples[1], -0.5F);
    NEAR(doubled.samples[3], 0.25F);
    NEAR(doubled.samples[7], source.samples.back());
    auto stereo = encode_wav({2, {-1, 1, 0, 0, 0.5F, -0.5F}});
    put(stereo, 24, 44100);
    put(stereo, 28, 44100 * 4);
    const auto converted = decode_wav(stereo);
    CHECK(converted.samples.size() == 8);
    NEAR(converted.samples[2], -1.0F + 44100.0F / 48000);
    NEAR(converted.samples[5], -converted.samples[4]);
    CHECK(decode_wav(fixture(22050)).samples.size() == 9);
}
TEST(mixer_loop_boundaries_pause_stop_and_stale_handles) {
    const SoundData sound{1, {0.25F, 0.5F, -0.25F}};
    Mixer mixer;
    const auto voice = mixer.play(sound.view(), {1, 0, true});
    std::array<float, 16> out{};
    mixer.mix(out);
    for (std::size_t i = 0; i < 8; ++i)
        NEAR(out[2 * i], sound.samples[i % 3]);
    CHECK(mixer.completed() == 0 && mixer.playing(voice));
    CHECK(mixer.pause(voice, true));
    mixer.mix(out);
    for (float value : out)
        NEAR(value, 0.0F);
    CHECK(mixer.pause(voice, false));
    mixer.mix(std::span(out).first(2));
    NEAR(out[0], -0.25F);
    CHECK(mixer.stop(voice));
    CHECK(!mixer.stop(voice));
    const auto replacement = mixer.play(sound.view(), {1});
    CHECK(replacement.serial != voice.serial);
    CHECK(!mixer.pause(voice, true));
    CHECK(!mixer.set_gain(voice, 0));
    mixer.mix(out);
    CHECK(mixer.completed() == 1 && !mixer.playing(replacement));
    NEAR(out[6], 0.0F);
    CHECK(mixer.stopped() == 1);
}
TEST(mixer_stereo_pan_gains_and_group_pause) {
    const SoundData stereo{2, {0.8F, 0.4F}}, mono{1, {0.25F}};
    Mixer mixer;
    const auto music = mixer.play(stereo.view(), {1, -1, true, AudioGroup::music});
    const auto effect = mixer.play(mono.view(), {1, 1, true, AudioGroup::effects});
    std::array<float, 128> out{};
    mixer.mix(out);
    NEAR(out[0], 0.8F);
    NEAR(out[1], 0.25F);
    CHECK(mixer.set_pan(music, 0));
    CHECK(mixer.set_group_gain(AudioGroup::music, 0.5F));
    CHECK(mixer.set_gain(effect, 0.5F));
    CHECK(mixer.set_master_gain(0.5F));
    mixer.mix(out);
    NEAR(out[126], 0.2F);
    NEAR(out[127], 0.1625F);
    CHECK(mixer.pause_group(AudioGroup::music, true));
    mixer.mix(out);
    NEAR(out[0], 0.0F);
    NEAR(out[1], 0.0625F);
    CHECK(mixer.set_master_gain(0));
    mixer.mix(out);
    NEAR(out[127], 0.0F);
    CHECK(mixer.active_voices() == 2); // Muting doesn't stop or pause voices.
    mixer.stop_all();
    CHECK(mixer.active_voices() == 0);
    CHECK(!mixer.set_master_gain(2));
    CHECK(!mixer.set_group_gain(AudioGroup::count, 1));
}
TEST(mixer_ramps_and_output_are_block_size_independent) {
    SoundData sound{1, {0.2F, 0.4F, 0.6F, 0.8F}};
    Mixer a, b;
    auto x = a.play(sound.view(), {1, 0, true}), y = b.play(sound.view(), {1, 0, true});
    a.set_gain(x, 0.2F);
    b.set_gain(y, 0.2F);
    a.set_master_gain(0.5F);
    b.set_master_gain(0.5F);
    std::array<float, 400> whole{}, pieces{};
    a.mix(whole);
    for (std::size_t i = 0; i < pieces.size(); i += 2)
        b.mix(std::span(pieces).subspan(i, 2));
    CHECK(whole == pieces);
    NEAR(whole[126], 0.08F);
    NEAR(whole[0], 0.2F * (1 - 0.8F / 64) * (1 - 0.5F / 64));
}
TEST(command_queue_fifo_pressure_and_voice_exhaustion) {
    using namespace audio_detail;
    CommandQueue queue;
    const SoundData sound{1, {0.25F}};
    Mixer mixer;
    Command play{};
    play.kind = CommandKind::play;
    play.voice = ticket();
    play.sound = sound.view();
    play.options = {1, 0, true};
    CHECK(queue.push(play));
    Command pause{};
    pause.kind = CommandKind::pause;
    pause.voice = play.voice;
    pause.flag = true;
    CHECK(queue.push(pause));
    Command command{};
    while (queue.pop(command))
        CHECK(command.apply(mixer));
    std::array<float, 2> out{};
    mixer.mix(out);
    NEAR(out[0], 0.0F);
    for (std::size_t cycle = 0; cycle < 5; ++cycle) {
        for (std::size_t i = 0; i < CommandQueue::capacity; ++i) {
            Command c{};
            c.kind = CommandKind::pan;
            c.voice = play.voice;
            c.value = static_cast<float>(i);
            CHECK(queue.push(c));
        }
        CHECK(!queue.push(play));
        for (std::size_t i = 0; i < CommandQueue::capacity; ++i) {
            CHECK(queue.pop(command));
            NEAR(command.value, static_cast<float>(i));
        }
        CHECK(!queue.pop(command));
    }
    mixer.stop_all();
    for (std::size_t i = 0; i < Mixer::max_voices; ++i)
        CHECK(mixer.play(sound.view(), {1, 0, true}));
    CHECK(!mixer.play(sound.view(), {1, 0, true}));
    mixer.mix(out);
    NEAR(out[0], 1.0F);
}
TEST(shipped_wav_assets_decode_and_mix_together) {
    AssetRoot root(TEST_ASSET_ROOT);
    std::array<SoundData, 4> sounds;
    const std::array<std::string_view, 4> names{"sounds/pickup.wav", "sounds/music.wav",
                                                "sounds/machine.wav", "sounds/door.wav"};
    Mixer mixer;
    for (std::size_t i = 0; i < sounds.size(); ++i) {
        sounds[i] = decode_wav(root.read(names[i], SoundData::max_file_bytes), names[i]);
        sounds[i].validate();
        CHECK(mixer.play(sounds[i].view(), {0.2F, 0, i == 1 || i == 2}));
    }
    CHECK(sounds[1].channels == 2);
    std::vector<float> output(48000 * 2);
    mixer.mix(output);
    CHECK(mixer.completed() == 2 && mixer.active_voices() == 2);
    CHECK(std::any_of(output.begin(), output.end(), [](float s) { return std::abs(s) > 0.01F; }));
    for (float sample : output)
        CHECK(std::isfinite(sample) && std::abs(sample) <= 1);
}
} // namespace
int main() {
    return testing::run_tests();
}
