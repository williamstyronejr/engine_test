#pragma once
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace engine {
// Immutable while in use. Interleaved mono/stereo float PCM at 48 kHz.
struct SoundView {
    std::span<const float> samples;
    std::uint32_t channels{1};
    std::size_t frames() const { return channels ? samples.size() / channels : 0; }
    bool valid() const {
        return (channels == 1 || channels == 2) && !samples.empty() &&
               samples.size() % channels == 0;
    }
};
struct SoundData {
    static constexpr std::uint32_t sample_rate = 48000;
    static constexpr std::size_t max_frames = sample_rate * 60;
    static constexpr std::size_t max_file_bytes = 32 * 1024 * 1024;
    std::uint32_t channels{1};
    std::vector<float> samples;
    SoundView view() const { return {samples, channels}; }
    void validate() const;
};
// PCM16 RIFF/WAVE, mono/stereo, 22050/24000/44100/48000 Hz; converted at load time.
SoundData decode_wav(std::span<const std::uint8_t> bytes, std::string_view source = "<wav>");
std::vector<std::uint8_t> encode_wav(const SoundData& sound); // Canonical PCM16 at 48 kHz.
} // namespace engine
