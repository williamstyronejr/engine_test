#include "../examples/feature_lab/asset_validation.hpp"
#include <iomanip>
#include <iostream>
#include <string_view>

int main(int argc, char** argv) {
    constexpr std::string_view help =
        "Usage: validate_assets [--assets DIRECTORY]\n"
        "Validate Feature Lab's shipped assets, texture references and both levels.\n"
        "Uses assets beside the executable or ../share/feature_lab by default.\n"
        "Read-only; no display/audio device or fallback assets. Shader sources are\n"
        "checked for readability only; OpenGL compilation and playability need runtime tests.\n"
        "Exit codes: 0 valid, 1 content/root failure, 2 invalid arguments.\n";
    std::filesystem::path directory;
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << help;
        return 0;
    }
    if (argc != 1 && (argc != 3 || std::string_view(argv[1]) != "--assets" || !*argv[2])) {
        std::cerr << help;
        return 2;
    }
    if (argc == 3)
        directory = argv[2];
    try {
        const auto assets = engine::AssetRoot::discover(directory);
        const auto report = feature_lab::validate_assets(assets);
        std::cout << "Asset root: " << assets.directory() << '\n';
        for (const auto& check : report.checks) {
            std::cout << (check.error.empty() ? "PASS " : "FAIL ") << std::quoted(check.name);
            if (!check.error.empty())
                std::cout << ": " << std::quoted(check.error);
            std::cout << '\n';
        }
        const auto failures = report.failures();
        std::cout << "Asset validation: " << report.checks.size() << " checks, " << failures
                  << " failed\nShader compilation, rendering, audio output and route completion "
                     "require runtime tests.\n";
        return failures ? 1 : 0;
    } catch (const std::exception& error) {
        std::cerr << "Asset validation failed: " << std::quoted(error.what()) << '\n';
        return 1;
    }
}
