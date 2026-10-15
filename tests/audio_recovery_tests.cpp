#include "../engine/src/audio_transport.hpp"
#include "engine/mixer.hpp"
#include "test.hpp"
#include <deque>
#include <vector>

namespace {
using namespace engine::audio_detail;
using namespace testing;
struct Device {
    std::deque<long> writes;
    std::deque<int> waits, resumes, prepares;
    std::vector<float> accepted;
    std::vector<std::size_t> requested;
    unsigned write_calls{}, wait_calls{}, resume_calls{}, prepare_calls{}, backoffs{};
    template <class T> static T take(std::deque<T>& values) {
        CHECK(!values.empty());
        const auto value = values.front();
        values.pop_front();
        return value;
    }
    long write(const float* data, std::size_t frames) {
        ++write_calls;
        requested.push_back(frames);
        const auto result = take(writes);
        if (result > 0 && static_cast<std::size_t>(result) <= frames)
            accepted.insert(accepted.end(), data, data + result * 2);
        return result;
    }
    int wait() {
        ++wait_calls;
        return take(waits);
    }
    int resume() {
        ++resume_calls;
        return take(resumes);
    }
    int prepare() {
        ++prepare_calls;
        return take(prepares);
    }
    void backoff() { ++backoffs; }
};
BlockWriter::Buffer samples() {
    BlockWriter::Buffer result{};
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<float>(i) / static_cast<float>(result.size());
    return result;
}
void complete(BlockWriter& writer, Device& device, const BlockWriter::Buffer& buffer) {
    for (int i = 0; i < 100 && !writer.needs_block() && !writer.stats().error; ++i)
        writer.step(device, buffer);
    CHECK(writer.needs_block() && !writer.stats().error);
    CHECK(writer.stats().written_frames == BlockWriter::frames);
    CHECK(device.accepted == std::vector<float>(buffer.begin(), buffer.end()));
}
TEST(partial_writes_waits_and_interrupts_preserve_every_stereo_sample) {
    BlockWriter writer;
    Device device;
    const auto buffer = samples();
    writer.step(device, buffer); // No block submitted yet.
    CHECK(device.write_calls == 0);
    writer.begin_block();
    device.writes = {7, -EINTR, -EAGAIN, 0, 1, 248};
    device.waits = {0, -EINTR};
    complete(writer, device, buffer);
    CHECK(device.requested == std::vector<std::size_t>({256, 249, 249, 249, 249, 248}));
    CHECK(writer.stats().waits == 2 && writer.stats().recoveries == 0);
    writer.step(device, buffer);
    CHECK(device.write_calls == 6); // Completion never writes stale samples again.
}
TEST(underruns_from_write_and_wait_prepare_without_replaying_accepted_prefix) {
    for (bool from_wait : {false, true}) {
        BlockWriter writer;
        Device device;
        writer.begin_block();
        device.writes = {11, from_wait ? -EAGAIN : -EPIPE, 245};
        if (from_wait)
            device.waits = {-EPIPE};
        device.prepares = {0};
        complete(writer, device, samples());
        CHECK(writer.stats().underruns == 1 && writer.stats().recoveries == 1);
        CHECK(device.prepare_calls == 1 && device.resume_calls == 0);
    }
}
TEST(suspend_retries_are_bounded_and_resume_preserves_pending_tail) {
    for (bool from_wait : {false, true}) {
        BlockWriter writer;
        Device device;
        writer.begin_block();
        device.writes = {19, from_wait ? -EAGAIN : -ESTRPIPE, 237};
        if (from_wait)
            device.waits = {-ESTRPIPE};
        device.resumes = {-EAGAIN, -EINTR, -EAGAIN, 0};
        const auto buffer = samples();
        writer.step(device, buffer);
        writer.step(device, buffer);
        for (int i = 0; i < 3; ++i) {
            writer.step(device, buffer);
            CHECK(device.resume_calls == static_cast<unsigned>(i + 1));
            CHECK(device.backoffs == device.resume_calls && device.write_calls == 2);
            CHECK(!writer.needs_block() && writer.stats().written_frames == 19);
        }
        complete(writer, device, buffer);
        CHECK(writer.stats().suspensions == 1 && writer.stats().recoveries == 1);
        CHECK(device.prepare_calls == 0 && device.resume_calls == 4);
    }
}
TEST(unsupported_resume_falls_back_to_prepare_once) {
    BlockWriter writer;
    Device device;
    writer.begin_block();
    device.writes = {-ESTRPIPE, 256};
    device.resumes = {-ENOSYS};
    device.prepares = {0};
    complete(writer, device, samples());
    CHECK(writer.stats().suspensions == 1 && writer.stats().recoveries == 1);
    CHECK(device.resume_calls == 1 && device.prepare_calls == 1);
}
TEST(device_and_prepare_failures_latch_original_error_and_stop_calls) {
    for (int scenario = 0; scenario < 4; ++scenario) {
        BlockWriter writer;
        Device device;
        writer.begin_block();
        const auto buffer = samples();
        if (scenario == 0)
            device.writes = {-ENODEV};
        else if (scenario == 1) {
            device.writes = {-EAGAIN};
            device.waits = {-ENODEV};
        } else if (scenario == 2) {
            device.writes = {-EPIPE};
            device.prepares = {-ENODEV};
        } else {
            device.writes = {-ESTRPIPE};
            device.resumes = {-ENOSYS};
            device.prepares = {-ENODEV};
        }
        for (int i = 0; i < 4; ++i)
            writer.step(device, buffer);
        CHECK(writer.stats().error == -ENODEV && !writer.needs_block());
        CHECK(writer.stats().recoveries == 0 && writer.stats().written_frames == 0);
        CHECK(device.write_calls == 1);
    }
}
TEST(invalid_backend_write_count_is_rejected_without_advancing) {
    BlockWriter writer;
    Device device;
    writer.begin_block();
    device.writes = {257};
    writer.step(device, samples());
    CHECK(writer.stats().error == -EIO && writer.stats().written_frames == 0);
}
TEST(recovery_does_not_remix_or_advance_voice_cursors) {
    engine::Mixer mixer;
    const std::array<float, 4> clip{.1F, .2F, .3F, .4F};
    CHECK(mixer.play(engine::SoundView{clip, 1}, {1, 0, true}));
    BlockWriter writer;
    Device device;
    device.writes = {31, -ESTRPIPE, 225};
    device.resumes = {-EAGAIN, 0};
    BlockWriter::Buffer buffer{};
    unsigned blocks = 0;
    for (int i = 0; i < 6; ++i) {
        if (writer.needs_block()) {
            mixer.mix(buffer);
            writer.begin_block();
            ++blocks;
        }
        writer.step(device, buffer);
        if (writer.needs_block())
            break;
    }
    CHECK(blocks == 1 && writer.needs_block());
    CHECK(device.accepted == std::vector<float>(buffer.begin(), buffer.end()));
    CHECK(mixer.started() == 1 && mixer.active_voices() == 1);
}
} // namespace
int main() {
    return testing::run_tests();
}
