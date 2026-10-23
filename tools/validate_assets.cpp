#include "../examples/feature_lab/asset_validation.hpp"
#include <array>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string_view>

namespace {
constexpr std::string_view help =
    "Usage: validate_assets [--assets DIRECTORY] [--json]\n"
    "       validate_assets [--assets DIRECTORY] --type TYPE --file KEY [--scene-budget N] [--json]\n"
    "       validate_assets --help\n"
    "Default: validate Feature Lab's shipped assets, references and both levels.\n"
    "Single file: TYPE is scene, tilemap, animation, texture, font or sound.\n"
    "KEY is relative to the asset root. Only the chosen format is checked;\n"
    "references and gameplay are not checked. Extensions do not select the format.\n"
    "Scene budget: 0..4096, default 4096; only valid with --type scene.\n"
    "--json writes one engine_asset_validation_v1 report to stdout. Argument errors\n"
    "remain text on stderr (exit 2); content/root failures produce JSON (exit 1).\n"
    "Uses assets beside the executable or ../share/feature_lab by default.\n"
    "Read-only; no display/audio device or fallback assets. Bundle shader checks\n"
    "read sources only; OpenGL compilation and playability need runtime tests.\n"
    "Exit codes: 0 valid, 1 content/root failure, 2 invalid arguments.\n";
using feature_lab::AssetFormat;
constexpr std::array formats{
    std::pair{std::string_view("scene"), AssetFormat::scene},
    std::pair{std::string_view("tilemap"), AssetFormat::tilemap},
    std::pair{std::string_view("animation"), AssetFormat::animation},
    std::pair{std::string_view("texture"), AssetFormat::texture},
    std::pair{std::string_view("font"), AssetFormat::font},
    std::pair{std::string_view("sound"), AssetFormat::sound}};
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << help;
        return 0;
    }
    std::filesystem::path directory;
    std::optional<AssetFormat> format;
    std::optional<std::string> file;
    std::optional<std::size_t> budget;
    std::string_view type_name;
    bool root_seen = false, json_output = false;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string_view option = argv[i];
            if (option == "--json") {
                if (json_output)
                    throw std::invalid_argument("Repeated option: --json");
                json_output = true;
                continue;
            }
            if (i + 1 >= argc || !*argv[i + 1])
                throw std::invalid_argument("Missing option value");
            const std::string_view value = argv[++i];
            if (option == "--assets" && !root_seen) {
                directory = value;
                root_seen = true;
            } else if (option == "--file" && !file) {
                file = value;
            } else if (option == "--type" && !format) {
                for (const auto& [name, kind] : formats)
                    if (name == value) {
                        format = kind;
                        type_name = name;
                    }
                if (!format)
                    throw std::invalid_argument("Unknown asset type");
            } else if (option == "--scene-budget" && !budget) {
                std::size_t count{};
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), count);
                if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
                    count > engine::Scene::capacity)
                    throw std::invalid_argument("Scene budget must be a decimal integer in 0..4096");
                budget = count;
            } else {
                throw std::invalid_argument("Unknown or repeated option: " + std::string(option));
            }
        }
        if (format.has_value() != file.has_value())
            throw std::invalid_argument("--type and --file must be used together");
        if (budget && format != AssetFormat::scene)
            throw std::invalid_argument("--scene-budget requires --type scene and --file");
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n' << help;
        return 2;
    }
    feature_lab::AssetValidationContext context;
    context.format = format;
    context.file = file.value_or("");
    context.scene_limits = {budget.value_or(engine::Scene::capacity)};
    if (root_seen)
        context.requested_root = directory.native();
    const auto emit_json = [&](const feature_lab::AssetValidation& report,
                               std::optional<std::string> fatal = std::nullopt) {
        const auto serialized = feature_lab::asset_validation_json(report, context, fatal);
        std::cout << serialized << '\n';
        std::cout.flush();
        if (!std::cout) {
            std::cerr << "Cannot write asset validation report\n";
            return 1;
        }
        return fatal || report.failures() ? 1 : 0;
    };
    try {
        const auto assets = engine::AssetRoot::discover(directory);
        context.asset_root = assets.directory().native();
        const engine::Scene::Limits limits{budget.value_or(engine::Scene::capacity)};
        const auto report = format ? feature_lab::validate_asset_file(assets, *format, *file, limits)
                                   : feature_lab::validate_assets(assets);
        if (json_output)
            return emit_json(report);
        std::cout << "Asset root: " << assets.directory() << '\n';
        if (format) {
            std::cout << "Validation mode: " << type_name << " format";
            if (*format == AssetFormat::scene)
                std::cout << " (scene budget=" << limits.entities << ')';
            std::cout << "\nReferences and gameplay are not checked.\n";
        }
        for (const auto& check : report.checks) {
            std::cout << (check.error.empty() ? "PASS " : "FAIL ") << std::quoted(check.name);
            if (!check.error.empty())
                std::cout << ": " << std::quoted(check.error);
            std::cout << '\n';
        }
        const auto failures = report.failures();
        std::cout << "Asset validation: " << report.checks.size()
                  << (report.checks.size() == 1 ? " check, " : " checks, ") << failures
                  << " failed\nShader compilation, rendering, audio output and route completion "
                     "require runtime tests.\n";
        return failures ? 1 : 0;
    } catch (const std::exception& error) {
        if (json_output)
            return emit_json({}, std::string(error.what()));
        std::cerr << "Asset validation failed: " << std::quoted(error.what()) << '\n';
        return 1;
    }
}
