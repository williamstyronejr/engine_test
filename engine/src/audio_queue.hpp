#pragma once
#include "engine/mixer.hpp"
#include <array>
#include <atomic>

namespace engine::audio_detail {
enum class CommandKind { play, stop, pause, gain, pan, master, group_gain, group_pause, stop_all };
struct Command {
    CommandKind kind{};
    VoiceHandle voice;
    SoundView sound;
    Playback options;
    float value{};
    bool flag{};
    AudioGroup group{};
    bool apply(Mixer& mixer) const {
        switch (kind) {
        case CommandKind::play:
            return mixer.start(voice, sound, options);
        case CommandKind::stop:
            return mixer.stop(voice);
        case CommandKind::pause:
            return mixer.pause(voice, flag);
        case CommandKind::gain:
            return mixer.set_gain(voice, value);
        case CommandKind::pan:
            return mixer.set_pan(voice, value);
        case CommandKind::master:
            return mixer.set_master_gain(value);
        case CommandKind::group_gain:
            return mixer.set_group_gain(group, value);
        case CommandKind::group_pause:
            return mixer.pause_group(group, flag);
        case CommandKind::stop_all:
            mixer.stop_all();
            return true;
        }
        return false;
    }
};
// One producer and one consumer. Commands borrow immutable, output-owned samples.
class CommandQueue {
  public:
    static constexpr std::size_t capacity = 63;
    bool push(const Command& command) {
        const auto w = write_.load(std::memory_order_relaxed);
        const auto next = (w + 1) % slots;
        if (next == read_.load(std::memory_order_acquire))
            return false;
        commands_[w] = command;
        write_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(Command& command) {
        const auto r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire))
            return false;
        command = commands_[r];
        read_.store((r + 1) % slots, std::memory_order_release);
        return true;
    }

  private:
    static constexpr std::size_t slots = capacity + 1;
    static_assert(std::atomic<std::size_t>::is_always_lock_free);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::array<Command, slots> commands_{};
    alignas(64) std::atomic<std::size_t> write_{};
    alignas(64) std::atomic<std::size_t> read_{};
};
} // namespace engine::audio_detail
