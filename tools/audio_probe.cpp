#include "engine/audio.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
#include <string>
#include <thread>

namespace {
using namespace std::chrono_literals;
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void hold(engine::AudioOutput& audio, std::chrono::milliseconds duration) {
    const auto end = std::chrono::steady_clock::now() + duration;
    do {
        if (!audio.healthy())
            throw std::runtime_error("Audio device failed: " +
                                     std::to_string(audio.stats().device_error));
        std::this_thread::sleep_for(10ms);
    } while (std::chrono::steady_clock::now() < end);
}
void report(const engine::AudioOutput& audio) {
    const auto s = audio.stats();
    std::cout << "[audio probe] healthy=" << audio.healthy()
              << " written_frames=" << s.written_frames << " underruns=" << s.underruns
              << " suspensions=" << s.suspensions << " recoveries=" << s.recoveries
              << " waits=" << s.waits << " device_error=" << s.device_error
              << " started=" << s.started << " stopped=" << s.stopped
              << " active=" << s.active_voices << std::endl;
}
} // namespace
int main(int argc, char** argv) {
    try {
        bool exercise = false, named = false;
        std::string device = "default";
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i];
            if (arg == "--help") {
                std::cout << "audio_probe [DEVICE | --device DEVICE] [--exercise]\n"
                             "Default: silent initialization/progress check.\n"
                             "--exercise: about 8 seconds of low-amplitude 440 Hz tones:\n"
                             "left, right, center, muted, restored, paused, resumed, stopped.\n"
                             "Logs confirm software output only; listen to verify speakers.\n"
                             "No system volume or device settings are changed.\n";
                return 0;
            }
            if (arg == "--exercise" && !exercise)
                exercise = true;
            else if (arg == "--device" && !named && i + 1 < argc) {
                device = argv[++i];
                named = true;
            } else if (!arg.starts_with('-') && !named) {
                device = arg;
                named = true;
            } else
                throw std::invalid_argument("Invalid audio_probe arguments; use --help");
        }
        require(!device.empty(), "Audio device name is empty");
        engine::AudioOutput audio(device, {.5F, 1, 1});
        try {
            hold(audio, 150ms);
            require(audio.stats().written_frames > 0, "Device accepted no PCM frames");
            if (exercise) {
                engine::SoundData tone{1, std::vector<float>(4800)};
                for (std::size_t i = 0; i < tone.samples.size(); ++i)
                    tone.samples[i] = .12F * std::sin(2 * std::numbers::pi_v<float> * 440 *
                                                      static_cast<float>(i) / 48000);
                const auto sound = audio.register_sound(std::move(tone));
                const auto voice = audio.play(sound, {1, -1, true});
                require(static_cast<bool>(voice), "Tone play rejected");
                const auto phase = [&](const char* text) {
                    std::cout << "[listen] " << text << std::endl;
                    hold(audio, 1s);
                    report(audio);
                };
                phase("LEFT ONLY");
                require(audio.set_pan(voice, 1), "Pan rejected");
                phase("RIGHT ONLY");
                require(audio.set_pan(voice, 0), "Pan rejected");
                phase("CENTER / BOTH CHANNELS");
                require(audio.set_master_gain(0), "Mute rejected");
                phase("MUTED / SILENCE");
                require(audio.set_master_gain(.5F), "Unmute rejected");
                phase("RESTORED / BOTH CHANNELS");
                require(audio.pause_group(engine::AudioGroup::effects, true), "Pause rejected");
                phase("GROUP PAUSED / SILENCE");
                require(audio.pause_group(engine::AudioGroup::effects, false), "Resume rejected");
                phase("GROUP RESUMED / BOTH CHANNELS");
                require(audio.stop_all(), "Stop rejected");
                phase("STOPPED / SILENCE");
                const auto stats = audio.stats();
                require(stats.started == 1 && stats.stopped == 1 && stats.active_voices == 0 &&
                            stats.queue_rejections == 0 && stats.stale_controls == 0,
                        "Unexpected voice/control counters");
            }
            report(audio);
            std::cout << "48 kHz stereo float PCM submitted. Physical audibility/disconnect "
                         "recovery requires listener/device checks.\n";
        } catch (...) {
            report(audio);
            throw;
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
