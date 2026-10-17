#include "../examples/feature_lab/replay_file.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        if (argc != 3)
            throw std::invalid_argument("Usage: make_profile_replay OUTPUT.erpl facility|annex");
        const std::string_view level(argv[2]);
        if (level != "facility" && level != "annex")
            throw std::invalid_argument("Unknown profile level");
        auto game = feature_lab::load_game(engine::AssetRoot(TEST_ASSET_ROOT),
                                           level == "facility" ? feature_lab::Level::facility
                                                               : feature_lab::Level::annex);
        feature_lab::ReplayRecorder recorder(game);
        feature_lab::Replay route;
        for (int i = 0; i < 3000; ++i) {
            engine::InputFrame input{};
            const auto press = [&](engine::Key key) {
                input[static_cast<std::size_t>(key)].pressed = true;
            };
            if (i == 1500) {
                if (!game.won)
                    throw std::runtime_error("Profile route did not finish before restart");
                press(engine::Key::restart);
                route = feature_lab::Replay{};
            } else if (i == 120 || i == 180)
                press(engine::Key::pause);
            else if (i == 150)
                press(engine::Key::single_step);
            else if (!game.paused && !game.won)
                input = route.next(game);
            if (i >= 30 && i < 40)
                input[static_cast<std::size_t>(engine::Key::zoom_in)].held = true;
            if (i >= 210 && i < 220)
                input[static_cast<std::size_t>(engine::Key::zoom_out)].held = true;
            recorder.step(game, input);
        }
        if (!game.won)
            throw std::runtime_error("Profile route did not finish after restart");
        feature_lab::write_replay(argv[1], recorder.file());
        std::cout << "commands=3000 ticks=" << game.ticks
                  << " hash=" << feature_lab::replay_hash(game) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
