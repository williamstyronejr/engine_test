#include "engine/audio.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    using namespace std::chrono;
    try {
        engine::AudioOutput output("default");
        const auto start = steady_clock::now();
        auto next = start;
        bool healthy = true;
        for (;;) {
            const auto now = steady_clock::now();
            healthy = output.healthy();
            if (now >= next || !healthy || now - start >= seconds(120)) {
                const auto s = output.stats();
                std::cout << "elapsed_ms=" << duration_cast<milliseconds>(now - start).count()
                          << " healthy=" << healthy << " error=" << s.device_error
                          << " written=" << s.written_frames << " waits=" << s.waits
                          << " underruns=" << s.underruns << " suspensions=" << s.suspensions
                          << " recoveries=" << s.recoveries << std::endl;
                next = now + seconds(1);
            }
            if (!healthy || now - start >= seconds(120))
                break;
            std::this_thread::sleep_for(milliseconds(10));
        }
        return healthy ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
