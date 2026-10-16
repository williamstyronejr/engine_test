#pragma once
#include "engine/mixer.hpp"
#include <memory>
#include <string_view>

namespace engine {
struct SoundHandle {
    std::uint32_t slot{};
    std::uint64_t serial{};
};
struct SoundReplacement {
    SoundHandle handle;
    SoundData data;
};
struct AudioStats {
    std::uint64_t processed{}, started{}, completed{}, stopped{}, dropped_plays{}, stale_controls{},
        queue_rejections{};
    std::size_t active_voices{};
    std::uint64_t written_frames{}, underruns{}, suspensions{}, recoveries{}, waits{};
    int device_error{}; // Latched negative ALSA error; zero while healthy.
};
class AudioOutput {
  public:
    explicit AudioOutput(std::string_view device = "default", MixerGains initial = {});
    ~AudioOutput();
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    static constexpr std::size_t max_sounds = 32, max_sound_bytes = 64 * 1024 * 1024;
    // Main-thread only. Transfer immutable sample ownership.
    SoundHandle register_sound(SoundData sound);
    // Validate/stage the whole batch, then enqueue one block-boundary replacement.
    // Handles remain valid. False means queue/device rejection; invalid data throws.
    // Active voices preserve controls and normalized position. Input data may move.
    bool replace_sounds(std::span<SoundReplacement> replacements);
    // Main-thread retirement after worker acknowledgement; never frees on worker.
    void collect_sounds();
    std::size_t sound_count() const;
    std::size_t sound_bytes() const;          // Current bank.
    std::size_t retained_sound_bytes() const; // Current + unacknowledged old samples.
    // Nonzero ticket means enqueued, not a guarantee that a voice slot was available.
    VoiceHandle play(SoundHandle sound, Playback options = {});
    bool stop(VoiceHandle voice);
    bool pause(VoiceHandle voice, bool paused);
    bool set_gain(VoiceHandle voice, float gain);
    bool set_pan(VoiceHandle voice, float pan);
    bool set_master_gain(float gain);
    bool set_group_gain(AudioGroup group, float gain);
    bool pause_group(AudioGroup group, bool paused);
    bool stop_all();
    AudioStats stats() const; // Eventual snapshot; counters need not be from the same block.
    bool healthy() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace engine
