#include "engine/audio.hpp"
#include <chrono>
#include <iostream>
#include <thread>
int main(int argc, char** argv) {
    try {
        engine::AudioOutput audio(argc > 1 ? argv[1] : "default");
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        if (!audio.healthy()) {
            std::cerr << "Audio output failed\n";
            return 1;
        }
        std::cout << "Audio device accepted 48 kHz stereo float PCM; silence submitted. Audible "
                     "output not verified.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
