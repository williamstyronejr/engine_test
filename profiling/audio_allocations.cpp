#include "../examples/feature_lab/soundtrack.hpp"
#include "../examples/feature_lab/stress_benchmark.hpp"
#include "allocation_profile.hpp"
#include "stress_build.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

namespace {
template <class F> void eventually(F ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready()) {
        if (std::chrono::steady_clock::now() >= deadline)
            throw std::runtime_error("Audio allocation workload timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 2)
            throw std::invalid_argument("Usage: audio_allocations REPORT.json");
        const engine::AssetRoot assets(TEST_ASSET_ROOT);
        auto game = feature_lab::load_game(assets);
        engine::AudioStats stats;
        {
            feature_lab::Soundtrack track(assets, true, "null");
            if (!track.enabled())
                throw std::runtime_error("ALSA null unavailable");
            track.locate_emitter(game);
            eventually([&] { return track.stats().started == 2; });
            feature_lab::Replay replay;
            for (int i = 0; i < 3000; ++i) {
                const auto input = replay.next(game);
                const auto picked = game.update(input);
                {
                    allocation_profile::Sample sample(allocation_profile::audio_producer);
                    track.update(game, input, picked);
                    track.collect();
                }
                // Null has no device clock; allow real worker commands to drain.
                if (i % 10 == 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            eventually([&] {
                return track.stats().started == static_cast<std::uint64_t>(3 + game.total()) &&
                       track.stats().active_voices == 1;
            });
            stats = track.stats();
            if (!game.won || !track.enabled() || stats.queue_rejections || stats.dropped_plays ||
                stats.stale_controls)
                throw std::runtime_error("Incomplete or rejected audio workload");
        } // Join worker before reading allocation aggregates or serializing the report.
        const auto mix = allocation_profile::summary(allocation_profile::audio_mix);
        const auto transport = allocation_profile::summary(allocation_profile::audio_transport);
        const bool complete = mix.samples > 0 && transport.samples > 0;
        std::ofstream out(argv[1]);
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out << "{\"schema\":\"feature_lab_audio_allocations_v1\",\"revision\":"
            << feature_lab::json_string(feature_lab::stress_build_revision)
            << ",\"compiler\":" << feature_lab::json_string(feature_lab::stress_build_compiler)
            << ",\"configuration\":"
            << feature_lab::json_string(feature_lab::stress_build_configuration)
            << ",\"workload\":\"facility_soundtrack_null_v1\",\"iterations\":3000,\"warmup\":0"
            << ",\"complete\":" << (complete ? "true" : "false")
            << ",\"won\":true,\"ticks\":" << game.ticks
            << ",\"device\":\"null\",\"stats_before_join\":{\"started\":" << stats.started
            << ",\"completed\":" << stats.completed << ",\"processed\":" << stats.processed
            << ",\"written_frames\":" << stats.written_frames
            << "},\"allocations\":" << allocation_profile::json() << "}\n";
        out.close();
        return complete && mix.calls == 0 && transport.calls == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
