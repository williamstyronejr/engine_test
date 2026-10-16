#include "../engine/src/audio_queue.hpp"
#include "test.hpp"

namespace {
using namespace engine;
using namespace testing;
TEST(replacement_preserves_phase_handles_and_stereo_channel_changes) {
    SoundData old{1, {.1F, .2F, .3F, .4F}}, next{2, {.5F, -.5F, .8F, -.8F}};
    Mixer mixer;
    const auto voice = mixer.play(old.view(), {1, 0, true});
    std::array<float, 4> output{};
    mixer.mix(output); // Cursor 2/4 maps to 1/2 in the new clip.
    const std::array swaps{SoundSwap{old.view(), next.view()}};
    CHECK(mixer.replace_sounds(swaps));
    mixer.mix(output);
    NEAR(output[0], .8F);
    NEAR(output[1], -.8F);
    NEAR(output[2], .5F);
    NEAR(output[3], -.5F);
    CHECK(mixer.playing(voice) && mixer.started() == 1 && mixer.completed() == 0);
    CHECK(mixer.stop(voice) && mixer.stopped() == 1);
}
TEST(paused_voices_groups_pan_and_gain_ramps_survive_without_restarts) {
    SoundData old{1, std::vector<float>(100, .2F)}, next{1, std::vector<float>(20, .4F)};
    Mixer mixer, reference;
    const auto a = mixer.play(old.view(), {1, -.5F, true, AudioGroup::music});
    const auto b = reference.play(old.view(), {1, -.5F, true, AudioGroup::music});
    mixer.set_gain(a, .25F);
    reference.set_gain(b, .25F);
    mixer.set_master_gain(.5F);
    reference.set_master_gain(.5F);
    mixer.set_group_gain(AudioGroup::music, .5F);
    reference.set_group_gain(AudioGroup::music, .5F);
    mixer.pause(a, true);
    reference.pause(b, true);
    const std::array swaps{SoundSwap{old.view(), next.view()}};
    CHECK(mixer.replace_sounds(swaps));
    std::array<float, 16> actual{}, expected{};
    mixer.mix(actual);
    reference.mix(expected);
    CHECK(actual == expected && actual[0] == 0);
    mixer.pause(a, false);
    reference.pause(b, false);
    mixer.pause_group(AudioGroup::music, true);
    reference.pause_group(AudioGroup::music, true);
    mixer.mix(actual);
    reference.mix(expected);
    CHECK(actual == expected && actual[0] == 0);
    mixer.pause_group(AudioGroup::music, false);
    reference.pause_group(AudioGroup::music, false);
    mixer.mix(actual);
    reference.mix(expected);
    for (std::size_t i = 0; i < actual.size(); ++i)
        NEAR(actual[i], expected[i] * 2);
    CHECK(mixer.started() == 1 && mixer.stopped() == 0);
}
TEST(one_shot_completion_is_exactly_once_and_never_resurrected) {
    SoundData old{1, {.1F, .2F, .3F, .4F}}, next{1, {.7F, .8F}};
    Mixer mixer;
    const auto voice = mixer.play(old.view(), {1});
    std::array<float, 6> first{};
    mixer.mix(first); // 3/4 -> 1/2.
    const std::array swaps{SoundSwap{old.view(), next.view()}};
    CHECK(mixer.replace_sounds(swaps));
    std::array<float, 4> last{};
    mixer.mix(last);
    NEAR(last[0], .8F);
    NEAR(last[2], 0);
    CHECK(!mixer.playing(voice) && mixer.completed() == 1 && mixer.started() == 1);
    CHECK(mixer.replace_sounds(swaps));
    mixer.mix(last);
    CHECK(last[0] == 0 && mixer.completed() == 1 && mixer.active_voices() == 0);
}
TEST(whole_batch_validation_and_simultaneous_swaps) {
    SoundData a{1, {.2F}}, b{1, {.7F}};
    Mixer mixer;
    const auto voice = mixer.play(a.view(), {1, 0, true});
    std::array bad{SoundSwap{a.view(), b.view()}, SoundSwap{b.view(), {}}};
    CHECK(!mixer.replace_sounds(bad));
    bad[1] = bad[0];
    CHECK(!mixer.replace_sounds(bad));
    CHECK(!mixer.replace_sounds({}));
    std::array<float, 2> output{};
    mixer.mix(output);
    NEAR(output[0], .2F);
    const std::array swaps{SoundSwap{a.view(), b.view()}, SoundSwap{b.view(), a.view()}};
    CHECK(mixer.replace_sounds(swaps));
    mixer.mix(output);
    NEAR(output[0], .7F);
    CHECK(mixer.playing(voice));
}
TEST(queue_orders_old_play_replacement_new_play_and_controls) {
    using namespace engine::audio_detail;
    SoundData old{1, {.2F}}, next{1, {.6F}};
    const std::array swaps{SoundSwap{old.view(), next.view()}};
    CommandQueue queue;
    Command play{};
    play.kind = CommandKind::play;
    play.voice = ticket();
    play.sound = old.view();
    play.options = {.5F, -1, true};
    CHECK(queue.push(play));
    Command reload{};
    reload.kind = CommandKind::replace_sounds;
    reload.replacements = swaps;
    CHECK(queue.push(reload));
    play.voice = ticket();
    play.sound = next.view();
    play.options.pan = 1;
    CHECK(queue.push(play));
    Mixer mixer;
    Command command{};
    while (queue.pop(command))
        CHECK(command.apply(mixer));
    std::array<float, 2> output{};
    mixer.mix(output);
    NEAR(output[0], .3F);
    NEAR(output[1], .3F);
    CHECK(mixer.started() == 2 && mixer.active_voices() == 2);
}
} // namespace
int main() {
    return testing::run_tests();
}
