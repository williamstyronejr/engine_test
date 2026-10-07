#pragma once
#include "engine/sound.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <stdexcept>

namespace engine {
enum class AudioGroup : std::size_t { music, effects, count };
struct VoiceHandle {
    std::uint64_t serial{};
    explicit operator bool() const { return serial != 0; }
};
struct MixerGains {
    float master{1}, music{1}, effects{1};
};
struct Playback {
    float gain{0.2F}, pan{};
    bool loop{};
    AudioGroup group{AudioGroup::effects};
};
namespace audio_detail {
inline VoiceHandle ticket() {
    static std::atomic<std::uint64_t> next{1};
    return {next.fetch_add(1, std::memory_order_relaxed)};
}
inline bool gain_valid(float value) {
    return std::isfinite(value) && value >= 0 && value <= 1;
}
inline bool group_valid(AudioGroup group) {
    return group < AudioGroup::count;
}
inline bool playback_valid(Playback p) {
    return gain_valid(p.gain) && std::isfinite(p.pan) && group_valid(p.group);
}
} // namespace audio_detail
// Thread-confined. Views must remain immutable/alive through playback. mix() does
// no allocation, locking, or I/O. Gain controls ramp over 64 output frames.
class Mixer {
  public:
    static constexpr std::size_t max_voices = 16, gain_ramp_frames = 64;
    explicit Mixer(MixerGains initial = {}) {
        for (float value : {initial.master, initial.music, initial.effects})
            if (!audio_detail::gain_valid(value))
                throw std::invalid_argument("Initial mixer gain outside [0,1]");
        master_ = {initial.master, initial.master, 0};
        groups_[0].gain = {initial.music, initial.music, 0};
        groups_[1].gain = {initial.effects, initial.effects, 0};
    }
    VoiceHandle play(SoundView sound, Playback options = {}) {
        const auto handle = audio_detail::ticket();
        return start(handle, sound, options) ? handle : VoiceHandle{};
    }
    bool play(std::span<const float> samples, float gain = 0.2F, float pan = 0) {
        return static_cast<bool>(play(SoundView{samples, 1}, {gain, pan}));
    }
    // Transport supplies a unique ticket before the asynchronous command is queued.
    bool start(VoiceHandle handle, SoundView sound, Playback options) {
        if (!handle || !sound.valid() || !audio_detail::playback_valid(options) || find(handle))
            return false;
        for (auto& voice : voices_)
            if (!voice.handle) {
                voice = {handle,
                         sound,
                         0,
                         {options.gain, options.gain, 0},
                         std::clamp(options.pan, -1.0F, 1.0F),
                         options.loop,
                         false,
                         options.group};
                ++started_;
                return true;
            }
        return false;
    }
    bool stop(VoiceHandle handle) {
        auto* v = find(handle);
        if (!v)
            return false;
        *v = {};
        ++stopped_;
        return true;
    }
    void stop_all() {
        for (auto& v : voices_)
            if (v.handle) {
                v = {};
                ++stopped_;
            }
    }
    bool pause(VoiceHandle handle, bool paused) {
        auto* v = find(handle);
        if (!v)
            return false;
        v->paused = paused;
        return true;
    }
    bool set_gain(VoiceHandle handle, float gain) {
        auto* v = find(handle);
        if (!v || !audio_detail::gain_valid(gain))
            return false;
        v->gain.set(gain);
        return true;
    }
    bool set_pan(VoiceHandle handle, float pan) {
        auto* v = find(handle);
        if (!v || !std::isfinite(pan))
            return false;
        v->pan = std::clamp(pan, -1.0F, 1.0F);
        return true;
    }
    bool set_master_gain(float gain) {
        if (!audio_detail::gain_valid(gain))
            return false;
        master_.set(gain);
        return true;
    }
    bool set_group_gain(AudioGroup group, float gain) {
        if (!audio_detail::group_valid(group) || !audio_detail::gain_valid(gain))
            return false;
        groups_[static_cast<std::size_t>(group)].gain.set(gain);
        return true;
    }
    bool pause_group(AudioGroup group, bool paused) {
        if (!audio_detail::group_valid(group))
            return false;
        groups_[static_cast<std::size_t>(group)].paused = paused;
        return true;
    }
    bool playing(VoiceHandle handle) const { return find(handle) != nullptr; }
    std::size_t active_voices() const {
        return static_cast<std::size_t>(
            std::count_if(voices_.begin(), voices_.end(),
                          [](const auto& v) { return static_cast<bool>(v.handle); }));
    }
    std::uint64_t started() const { return started_; }
    std::uint64_t completed() const { return completed_; }
    std::uint64_t stopped() const { return stopped_; }
    void mix(std::span<float> stereo) {
        if (stereo.size() % 2)
            throw std::invalid_argument("Stereo buffer must contain complete frames");
        for (std::size_t i = 0; i < stereo.size(); i += 2) {
            const float master = master_.next();
            std::array<float, 2> gains{groups_[0].gain.next(), groups_[1].gain.next()};
            float left = 0, right = 0;
            for (auto& v : voices_) {
                if (!v.handle)
                    continue;
                const auto group = static_cast<std::size_t>(v.group);
                const float gain = v.gain.next() * gains[group] * master;
                if (v.paused || groups_[group].paused)
                    continue;
                const auto offset = v.cursor * v.sound.channels;
                left += v.sound.samples[offset] * gain * (1 - std::max(v.pan, 0.0F));
                right += v.sound.samples[offset + (v.sound.channels == 2 ? 1 : 0)] * gain *
                         (1 + std::min(v.pan, 0.0F));
                if (++v.cursor == v.sound.frames()) {
                    if (v.loop)
                        v.cursor = 0;
                    else {
                        v = {};
                        ++completed_;
                    }
                }
            }
            stereo[i] = std::clamp(left, -1.0F, 1.0F);
            stereo[i + 1] = std::clamp(right, -1.0F, 1.0F);
        }
    }

  private:
    struct Ramp {
        float current{1}, target{1};
        std::size_t remaining{};
        void set(float value) {
            target = value;
            remaining = gain_ramp_frames;
        }
        float next() {
            if (remaining) {
                current += (target - current) / static_cast<float>(remaining);
                --remaining;
            }
            return current;
        }
    };
    struct Group {
        Ramp gain;
        bool paused{};
    };
    struct Voice {
        VoiceHandle handle;
        SoundView sound;
        std::size_t cursor{};
        Ramp gain;
        float pan{};
        bool loop{}, paused{};
        AudioGroup group{};
    };
    Voice* find(VoiceHandle h) {
        for (auto& v : voices_)
            if (h && v.handle.serial == h.serial)
                return &v;
        return nullptr;
    }
    const Voice* find(VoiceHandle h) const {
        for (const auto& v : voices_)
            if (h && v.handle.serial == h.serial)
                return &v;
        return nullptr;
    }
    std::array<Voice, max_voices> voices_{};
    std::array<Group, 2> groups_{};
    Ramp master_;
    std::uint64_t started_{}, completed_{}, stopped_{};
};
} // namespace engine
