#include "engine/sound.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

namespace engine {
void SoundData::validate() const {
    if (!view().valid() || view().frames() > max_frames)
        throw std::invalid_argument("Sound requires 1..2880000 mono/stereo frames");
    for (float sample : samples)
        if (!std::isfinite(sample) || sample < -1 || sample > 1)
            throw std::invalid_argument("Sound samples must be finite in [-1,1]");
}
SoundData decode_wav(std::span<const std::uint8_t> bytes, std::string_view source) {
    try {
        const auto require = [&](std::size_t offset, std::size_t size) {
            if (offset > bytes.size() || size > bytes.size() - offset)
                throw std::runtime_error("Truncated WAV at byte " + std::to_string(offset));
        };
        const auto u16 = [&](std::size_t offset) {
            require(offset, 2);
            return static_cast<std::uint16_t>(static_cast<unsigned>(bytes[offset]) |
                                              (static_cast<unsigned>(bytes[offset + 1]) << 8));
        };
        const auto u32 = [&](std::size_t offset) {
            require(offset, 4);
            std::uint32_t value = 0;
            for (unsigned i = 0; i < 4; ++i)
                value |= static_cast<std::uint32_t>(bytes[offset + i]) << (8 * i);
            return value;
        };
        const auto tag = [&](std::size_t offset, const char* text) {
            require(offset, 4);
            return std::memcmp(bytes.data() + offset, text, 4) == 0;
        };
        if (bytes.size() > SoundData::max_file_bytes)
            throw std::runtime_error("WAV exceeds 32 MiB");
        require(0, 12);
        if (!tag(0, "RIFF") || !tag(8, "WAVE") || u32(4) != bytes.size() - 8)
            throw std::runtime_error("Invalid RIFF/WAVE header or exact file length");
        std::size_t format = 0, payload = 0, payload_size = 0;
        bool found_data = false;
        for (std::size_t offset = 12; offset < bytes.size();) {
            require(offset, 8);
            const auto size = static_cast<std::size_t>(u32(offset + 4));
            const auto data = offset + 8;
            require(data, size);
            require(data + size, size & 1);
            if (tag(offset, "fmt ")) {
                if (format || (size != 16 && size != 18))
                    throw std::runtime_error("Duplicate/unsupported WAV format chunk");
                if (size == 18 && u16(data + 16) != 0)
                    throw std::runtime_error("Unsupported PCM extension");
                format = data;
            } else if (tag(offset, "data")) {
                if (found_data)
                    throw std::runtime_error("Duplicate WAV data chunk");
                found_data = true;
                payload = data;
                payload_size = size;
            }
            offset = data + size + (size & 1);
        }
        if (!format || !found_data)
            throw std::runtime_error("WAV needs fmt and data chunks");
        const auto channels = u16(format + 2);
        const auto rate = u32(format + 4);
        const auto block = u16(format + 12);
        if (u16(format) != 1 || u16(format + 14) != 16 || (channels != 1 && channels != 2))
            throw std::runtime_error("Only PCM16 mono/stereo WAV is supported");
        if (rate != 22050 && rate != 24000 && rate != 44100 && rate != 48000)
            throw std::runtime_error("WAV rate must be 22050, 24000, 44100 or 48000 Hz");
        if (block != channels * 2 || u32(format + 8) != rate * block || !payload_size ||
            payload_size % block)
            throw std::runtime_error("Invalid WAV byte rate, block alignment or data length");
        const auto frames = payload_size / block;
        if (frames > static_cast<std::size_t>(rate) * 60)
            throw std::runtime_error("WAV exceeds 60 seconds");
        const auto output_frames = static_cast<std::size_t>(
            (static_cast<std::uint64_t>(frames) * SoundData::sample_rate + rate - 1) / rate);
        SoundData result{channels, std::vector<float>(output_frames * channels)};
        const auto sample = [&](std::size_t frame, std::size_t channel) {
            return static_cast<float>(std::bit_cast<std::int16_t>(
                       u16(payload + (frame * channels + channel) * 2))) /
                   32768.0F;
        };
        for (std::size_t i = 0; i < output_frames; ++i) {
            const auto numerator = static_cast<std::uint64_t>(i) * rate;
            const auto a = static_cast<std::size_t>(numerator / SoundData::sample_rate);
            const auto b = std::min(a + 1, frames - 1);
            const float fraction =
                static_cast<float>(numerator % SoundData::sample_rate) / SoundData::sample_rate;
            for (std::size_t c = 0; c < channels; ++c) {
                const float first = sample(a, c);
                result.samples[i * channels + c] = first + (sample(b, c) - first) * fraction;
            }
        }
        return result;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string(source) + ": " + e.what());
    }
}
std::vector<std::uint8_t> encode_wav(const SoundData& sound) {
    sound.validate();
    std::vector<std::uint8_t> bytes;
    bytes.reserve(44 + sound.samples.size() * 2);
    const auto tag = [&](std::string_view s) {
        for (char c : s)
            bytes.push_back(static_cast<std::uint8_t>(c));
    };
    const auto integer = [&](std::uint32_t n, unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            bytes.push_back(static_cast<std::uint8_t>(n >> (i * 8)));
    };
    tag("RIFF");
    integer(static_cast<std::uint32_t>(36 + sound.samples.size() * 2), 4);
    tag("WAVEfmt ");
    integer(16, 4);
    integer(1, 2);
    integer(sound.channels, 2);
    integer(SoundData::sample_rate, 4);
    integer(SoundData::sample_rate * sound.channels * 2, 4);
    integer(sound.channels * 2, 2);
    integer(16, 2);
    tag("data");
    integer(static_cast<std::uint32_t>(sound.samples.size() * 2), 4);
    for (float sample : sound.samples) {
        const auto pcm =
            static_cast<std::int16_t>(std::clamp(std::lround(sample * 32768.0F), -32768L, 32767L));
        integer(std::bit_cast<std::uint16_t>(pcm), 2);
    }
    return bytes;
}
} // namespace engine
