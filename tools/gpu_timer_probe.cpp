// Diagnostic experiment, deliberately paced and drained. Never used by the game loop.
#include "../engine/src/gpu_timer_backend.hpp"
#include "../examples/feature_lab/stress_benchmark.hpp"
#include "engine/renderer.hpp"
#include "engine/window.hpp"
#include "stress_build.hpp"
#include <GL/glx.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    try {
        if (argc != 3)
            throw std::invalid_argument("Usage: gpu_timer_probe OUTPUT.json SECONDS (1..180)");
        std::size_t parsed{};
        const int seconds = std::stoi(argv[2], &parsed);
        if (parsed != std::string_view(argv[2]).size() || seconds < 1 || seconds > 180)
            throw std::invalid_argument("Probe duration must be 1..180 seconds");
        if (!std::getenv("DISPLAY"))
            return 77;
        engine::Window window(64, 64, "GPU timer rollover probe", false);
        engine::Renderer renderer;
        engine::detail::TimerBackend elapsed;
        engine::detail::TimerBackend timestamps(engine::GpuTimingMethod::timestamp_pair);
        if (elapsed.bits != 64 || timestamps.bits != 64)
            return 77;
        const auto flush = reinterpret_cast<PFNGLFLUSHPROC>(
            glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glFlush")));
        if (!flush)
            throw std::runtime_error("glFlush unavailable");
        const auto target = renderer.create_target(64, 64);
        const auto text = [&](GLenum name) {
            return feature_lab::json_string(
                reinterpret_cast<const char*>(elapsed.gl.GetString(name)));
        };
        std::ofstream out(argv[1]);
        out.exceptions(std::ios::failbit | std::ios::badbit);
        out << "{\"schema\":\"gpu_timer_rollover_probe_v1\",\"seconds_requested\":" << seconds
            << ",\"build_revision\":"
            << feature_lab::json_string(feature_lab::stress_build_revision)
            << ",\"vendor\":" << text(GL_VENDOR) << ",\"device\":" << text(GL_RENDERER)
            << ",\"version\":" << text(GL_VERSION)
            << ",\"timestamp_counter_bits\":" << timestamps.bits
            << ",\"elapsed_counter_bits\":" << elapsed.bits
            << ",\"pacing\":\"test_only_cpu_sleep_inside_query\",\"observations\":[\n";
        using Clock = std::chrono::steady_clock;
        const auto start = Clock::now();
        const auto deadline = start + std::chrono::seconds(seconds);
        constexpr std::uint64_t suspected_period = 1ULL << 36;
        std::uint64_t count{}, reversals{}, between{}, previous{}, largest_elapsed{};
        while (Clock::now() < deadline) {
            // Extend a diagnostic scope near the suspected boundary to catch it inside
            // the pair, instead of losing it in the gap between adjacent scopes.
            const bool near_boundary =
                previous < suspected_period && previous > suspected_period - 200000000;
            elapsed.begin(0);
            timestamps.begin(0);
            flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(near_boundary ? 250 : 5));
            renderer.begin(target, {{0, 0}, 2});
            renderer.quad({}, {2, 2}, {0, 1, 0, 1});
            renderer.end();
            timestamps.end(0);
            elapsed.end(0);
            flush();
            const auto ready_deadline = Clock::now() + std::chrono::seconds(3);
            while (!elapsed.available(0) || !timestamps.available(1) || !timestamps.available(0)) {
                if (Clock::now() > ready_deadline)
                    throw std::runtime_error("Probe query readiness timeout");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            const auto a = timestamps.result(0), b = timestamps.result(1);
            const auto duration = elapsed.result(0);
            largest_elapsed = std::max(largest_elapsed, duration);
            if (a < previous)
                ++between;
            if (b < a) {
                out << (reversals ? ",\n" : "") << "{\"sample\":" << count + 1 << ",\"start_ns\":\""
                    << a << "\",\"end_ns\":\"" << b << "\",\"elapsed_ns\":\"" << duration << "\"}";
                ++reversals;
            }
            previous = b;
            ++count;
            if (!renderer.healthy())
                throw std::runtime_error("Probe GL error");
        }
        out << "\n],\"samples\":" << count << ",\"reversed_pairs\":" << reversals
            << ",\"between_scope_reversals\":" << between << ",\"largest_elapsed_ns\":\""
            << largest_elapsed
            << "\",\"wall_seconds\":" << std::chrono::duration<double>(Clock::now() - start).count()
            << ",\"complete\":true}\n";
        out.close();
        std::cout << "Probe completed: " << count << " scopes, " << reversals
                  << " within-scope reversals, " << between << " between scopes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
