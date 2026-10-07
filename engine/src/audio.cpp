#include "engine/audio.hpp"
#include "audio_queue.hpp"
#include <alsa/asoundlib.h>
#include <array>
#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>

namespace engine {
using audio_detail::Command;
using audio_detail::CommandKind;
struct AudioOutput::Impl {
    explicit Impl(MixerGains initial) : mixer(initial) {}
    Mixer mixer; // Initialized before the worker starts; subsequently worker-owned.
    struct SoundSlot {
        std::unique_ptr<const SoundData> data;
        std::uint64_t serial{};
    };
    std::array<SoundSlot, max_sounds> sounds{}; // Main thread only; worker holds sample views.
    std::size_t count{}, bytes{};
    snd_pcm_t* pcm{};
    audio_detail::CommandQueue commands;
    std::atomic<std::uint64_t> processed{}, started{}, completed{}, stopped{}, dropped{}, stale{},
        rejected{};
    std::atomic<std::size_t> active{};
    std::atomic<bool> running{true}, failed{false};
    std::thread thread;
    ~Impl() {
        running.store(false);
        if (thread.joinable())
            thread.join();
        if (pcm) {
            snd_pcm_drop(pcm);
            snd_pcm_close(pcm);
        }
        // Sound storage is destroyed after the consumer has joined.
    }
    bool enqueue(Command command) {
        if (failed.load() || !commands.push(command)) {
            rejected.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    }
    void process() {
        std::array<float, 512> buffer{};
        std::size_t offset = buffer.size() / 2;
        std::uint64_t consumed = 0;
        while (running.load(std::memory_order_relaxed)) {
            if (offset == buffer.size() / 2) {
                Command command;
                // Bound work per block even if the producer keeps replenishing the queue.
                for (std::size_t n = 0;
                     n < audio_detail::CommandQueue::capacity && commands.pop(command); ++n) {
                    if (!command.apply(mixer)) {
                        if (command.kind == CommandKind::play)
                            dropped.fetch_add(1, std::memory_order_relaxed);
                        else
                            stale.fetch_add(1, std::memory_order_relaxed);
                    }
                    ++consumed;
                }
                mixer.mix(buffer);
                started.store(mixer.started(), std::memory_order_relaxed);
                completed.store(mixer.completed(), std::memory_order_relaxed);
                stopped.store(mixer.stopped(), std::memory_order_relaxed);
                active.store(mixer.active_voices(), std::memory_order_relaxed);
                processed.store(consumed, std::memory_order_release);
                offset = 0;
            }
            const auto n =
                snd_pcm_writei(pcm, buffer.data() + offset * 2, buffer.size() / 2 - offset);
            if (n > 0)
                offset += static_cast<std::size_t>(n);
            else if (n == -EAGAIN || n == 0)
                snd_pcm_wait(pcm, 10);
            else if (n == -EPIPE || n == -ESTRPIPE) {
                if (snd_pcm_prepare(pcm) < 0) {
                    failed.store(true);
                    break;
                }
            } else if (n != -EINTR) {
                failed.store(true);
                break;
            }
        }
    }
};
AudioOutput::AudioOutput(std::string_view device, MixerGains initial)
    : impl_(std::make_unique<Impl>(initial)) {
    const std::string name(device);
    int result = snd_pcm_open(&impl_->pcm, name.c_str(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (result < 0)
        throw std::runtime_error(std::string("Audio open: ") + snd_strerror(result));
    result = snd_pcm_set_params(impl_->pcm, SND_PCM_FORMAT_FLOAT_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                                2, 48000, 0, 40000);
    if (result < 0)
        throw std::runtime_error(std::string("Audio format: ") + snd_strerror(result));
    impl_->thread = std::thread([this] { impl_->process(); });
}
AudioOutput::~AudioOutput() = default;
SoundHandle AudioOutput::register_sound(SoundData sound) {
    sound.validate();
    auto& p = *impl_;
    const auto bytes = sound.samples.size() * sizeof(float);
    if (p.count == max_sounds || bytes > max_sound_bytes - p.bytes)
        throw std::runtime_error("Audio sound bank limit reached");
    static std::atomic<std::uint64_t> next_serial{1};
    const auto serial = next_serial.fetch_add(1, std::memory_order_relaxed);
    if (!serial)
        throw std::overflow_error("Sound handle serial exhausted");
    auto data = std::make_unique<const SoundData>(std::move(sound));
    const auto index = p.count;
    p.sounds[index] = {std::move(data), serial};
    ++p.count;
    p.bytes += bytes;
    return {static_cast<std::uint32_t>(index), serial};
}
std::size_t AudioOutput::sound_count() const {
    return impl_->count;
}
std::size_t AudioOutput::sound_bytes() const {
    return impl_->bytes;
}
VoiceHandle AudioOutput::play(SoundHandle sound, Playback options) {
    auto& p = *impl_;
    if (sound.slot >= p.count || !sound.serial || p.sounds[sound.slot].serial != sound.serial ||
        !audio_detail::playback_valid(options))
        return {};
    const auto voice = audio_detail::ticket();
    if (!voice)
        return {};
    Command command{};
    command.kind = CommandKind::play;
    command.voice = voice;
    command.sound = p.sounds[sound.slot].data->view();
    command.options = options;
    return p.enqueue(command) ? voice : VoiceHandle{};
}
bool AudioOutput::stop(VoiceHandle voice) {
    if (!voice)
        return false;
    Command c{};
    c.kind = CommandKind::stop;
    c.voice = voice;
    return impl_->enqueue(c);
}
bool AudioOutput::pause(VoiceHandle voice, bool paused) {
    if (!voice)
        return false;
    Command c{};
    c.kind = CommandKind::pause;
    c.voice = voice;
    c.flag = paused;
    return impl_->enqueue(c);
}
bool AudioOutput::set_gain(VoiceHandle voice, float gain) {
    if (!voice || !audio_detail::gain_valid(gain))
        return false;
    Command c{};
    c.kind = CommandKind::gain;
    c.voice = voice;
    c.value = gain;
    return impl_->enqueue(c);
}
bool AudioOutput::set_pan(VoiceHandle voice, float pan) {
    if (!voice || !std::isfinite(pan))
        return false;
    Command c{};
    c.kind = CommandKind::pan;
    c.voice = voice;
    c.value = pan;
    return impl_->enqueue(c);
}
bool AudioOutput::set_master_gain(float gain) {
    if (!audio_detail::gain_valid(gain))
        return false;
    Command c{};
    c.kind = CommandKind::master;
    c.value = gain;
    return impl_->enqueue(c);
}
bool AudioOutput::set_group_gain(AudioGroup group, float gain) {
    if (!audio_detail::group_valid(group) || !audio_detail::gain_valid(gain))
        return false;
    Command c{};
    c.kind = CommandKind::group_gain;
    c.group = group;
    c.value = gain;
    return impl_->enqueue(c);
}
bool AudioOutput::pause_group(AudioGroup group, bool paused) {
    if (!audio_detail::group_valid(group))
        return false;
    Command c{};
    c.kind = CommandKind::group_pause;
    c.group = group;
    c.flag = paused;
    return impl_->enqueue(c);
}
bool AudioOutput::stop_all() {
    Command c{};
    c.kind = CommandKind::stop_all;
    return impl_->enqueue(c);
}
AudioStats AudioOutput::stats() const {
    const auto& p = *impl_;
    return {p.processed.load(std::memory_order_acquire),
            p.started.load(),
            p.completed.load(),
            p.stopped.load(),
            p.dropped.load(),
            p.stale.load(),
            p.rejected.load(),
            p.active.load()};
}
bool AudioOutput::healthy() const {
    return !impl_->failed.load();
}
} // namespace engine
