#ifdef ENGINE_ALLOCATION_PROFILE
#include "../../profiling/allocation_profile.hpp"
#endif
#include "engine/audio.hpp"
#include "audio_queue.hpp"
#include "audio_transport.hpp"
#include <alsa/asoundlib.h>
#include <array>
#include <atomic>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>

namespace engine {
using audio_detail::Command;
using audio_detail::CommandKind;
namespace {
struct AlsaDevice {
    snd_pcm_t* pcm;
    snd_pcm_sframes_t write(const float* samples, std::size_t frames) {
        return snd_pcm_writei(pcm, samples, frames);
    }
    int wait() { return snd_pcm_wait(pcm, 10); }
    int prepare() { return snd_pcm_prepare(pcm); }
    int resume() { return snd_pcm_resume(pcm); }
    void backoff() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
};
} // namespace
struct AudioOutput::Impl {
    explicit Impl(MixerGains initial) : mixer(initial) {}
    Mixer mixer; // Initialized before the worker starts; subsequently worker-owned.
    struct SoundSlot {
        std::unique_ptr<const SoundData> data;
        std::uint64_t serial{};
    };
    std::array<SoundSlot, max_sounds> sounds{}; // Main thread only; worker holds sample views.
    std::size_t count{}, bytes{};
    struct Pending {
        std::vector<SoundSwap> swaps;
        std::vector<std::unique_ptr<const SoundData>> retired;
        std::size_t bytes{};
        std::uint64_t after{};
    };
    std::vector<std::unique_ptr<Pending>> pending; // Stable payload addresses until ack.
    std::size_t retired_bytes{};
    std::uint64_t submitted{}; // Producer only; matches worker's processed counter.
    snd_pcm_t* pcm{};
    audio_detail::CommandQueue commands;
    std::atomic<std::uint64_t> processed{}, started{}, completed{}, stopped{}, dropped{}, stale{},
        rejected{};
    std::atomic<std::size_t> active{};
    std::atomic<std::uint64_t> written_frames{}, underruns{}, suspensions{}, recoveries{}, waits{};
    std::atomic<int> device_error{};
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
        if (submitted == std::numeric_limits<std::uint64_t>::max() || failed.load() ||
            !commands.push(command)) {
            rejected.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        ++submitted;
        return true;
    }
    void collect() {
        const auto acknowledged = processed.load(std::memory_order_acquire);
        std::erase_if(pending, [&](const auto& batch) {
            if (batch->after > acknowledged)
                return false;
            retired_bytes -= batch->bytes;
            return true;
        });
    }
    void process() {
        audio_detail::BlockWriter::Buffer buffer{};
        audio_detail::BlockWriter writer;
        AlsaDevice device{pcm};
        std::uint64_t consumed = 0;
        while (running.load(std::memory_order_relaxed)) {
            if (writer.needs_block()) {
#ifdef ENGINE_ALLOCATION_PROFILE
                allocation_profile::Sample allocation_sample(allocation_profile::audio_mix);
#endif
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
                writer.begin_block();
            }
#ifdef ENGINE_ALLOCATION_PROFILE
            { allocation_profile::Sample allocation_sample(allocation_profile::audio_transport);
#endif
            writer.step(device, buffer);
#ifdef ENGINE_ALLOCATION_PROFILE
            }
#endif
            const auto transport = writer.stats();
            written_frames.store(transport.written_frames, std::memory_order_relaxed);
            underruns.store(transport.underruns, std::memory_order_relaxed);
            suspensions.store(transport.suspensions, std::memory_order_relaxed);
            recoveries.store(transport.recoveries, std::memory_order_relaxed);
            waits.store(transport.waits, std::memory_order_relaxed);
            if (transport.error) {
                device_error.store(transport.error, std::memory_order_relaxed);
                failed.store(true, std::memory_order_release);
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
    p.collect();
    const auto bytes = sound.samples.size() * sizeof(float);
    if (p.count == max_sounds || bytes > max_sound_bytes - p.bytes - p.retired_bytes)
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
bool AudioOutput::replace_sounds(std::span<SoundReplacement> replacements) {
    auto& p = *impl_;
    p.collect();
    if (replacements.empty() || replacements.size() > max_sounds)
        throw std::invalid_argument("Audio replacement batch must contain 1..32 sounds");
    std::array<bool, max_sounds> seen{};
    std::size_t new_bytes = 0, old_bytes = 0;
    for (const auto& entry : replacements) {
        const auto h = entry.handle;
        if (h.slot >= p.count || !h.serial || p.sounds[h.slot].serial != h.serial || seen[h.slot])
            throw std::invalid_argument("Invalid, foreign or duplicate replacement sound handle");
        seen[h.slot] = true;
        entry.data.validate();
        const auto bytes = entry.data.samples.size() * sizeof(float);
        if (bytes > max_sound_bytes - p.bytes - p.retired_bytes - new_bytes)
            throw std::runtime_error("Audio replacement exceeds retained sample budget");
        new_bytes += bytes;
        old_bytes += p.sounds[h.slot].data->samples.size() * sizeof(float);
    }
    auto batch = std::make_unique<Impl::Pending>();
    batch->swaps.reserve(replacements.size());
    batch->retired.reserve(replacements.size());
    std::vector<std::unique_ptr<const SoundData>> candidates;
    candidates.reserve(replacements.size());
    p.pending.reserve(p.pending.size() + 1);
    for (auto& entry : replacements) {
        candidates.push_back(std::make_unique<const SoundData>(std::move(entry.data)));
        batch->swaps.push_back(
            {p.sounds[entry.handle.slot].data->view(), candidates.back()->view()});
    }
    Command command{};
    command.kind = CommandKind::replace_sounds;
    command.replacements = batch->swaps;
    if (!p.enqueue(command))
        return false;
    // No throwing operations after enqueue. Worker can already use candidate samples;
    // all old samples stay owned until processed acknowledges this command/block.
    batch->after = p.submitted;
    batch->bytes = old_bytes;
    for (std::size_t i = 0; i < replacements.size(); ++i) {
        auto& slot = p.sounds[replacements[i].handle.slot];
        batch->retired.push_back(std::move(slot.data));
        slot.data = std::move(candidates[i]);
    }
    p.bytes = p.bytes - old_bytes + new_bytes;
    p.retired_bytes += old_bytes;
    p.pending.push_back(std::move(batch));
    return true;
}
void AudioOutput::collect_sounds() {
    impl_->collect();
}
std::size_t AudioOutput::retained_sound_bytes() const {
    return impl_->bytes + impl_->retired_bytes;
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
            p.active.load(),
            p.written_frames.load(),
            p.underruns.load(),
            p.suspensions.load(),
            p.recoveries.load(),
            p.waits.load(),
            p.device_error.load()};
}
bool AudioOutput::healthy() const {
    return !impl_->failed.load();
}
} // namespace engine
