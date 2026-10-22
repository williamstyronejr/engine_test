#include "asset_validation.hpp"
#include "game.hpp"
#include "engine/font.hpp"
#include "engine/renderer.hpp"
#include "engine/sound.hpp"
#include <map>
#include <numeric>
#include <optional>

namespace feature_lab {
std::size_t AssetValidation::failures() const {
    return static_cast<std::size_t>(std::count_if(checks.begin(), checks.end(),
                                                [](const auto& c) { return !c.error.empty(); }));
}
AssetValidation validate_assets(const engine::AssetRoot& assets) {
    AssetValidation result;
    const auto check = [&](std::string name, auto&& operation) {
        std::string error;
        try {
            operation();
        } catch (const std::exception& failure) {
            error = failure.what();
        }
        result.checks.push_back({std::move(name), std::move(error)});
    };
    // Inventory checks remain independent, so one bad level does not hide an
    // unrelated malformed file. No recursive scan or interpretation of extra files.
    for (const auto stem : {"facility", "annex"}) {
        const auto scene = std::string(stem) + ".scene", tile = std::string(stem) + ".etmp";
        check(scene, [&] { engine::parse_scene(assets.text(scene), scene, Game::scene_limits); });
        check(tile, [&] { engine::decode_tilemap(assets.read(tile, 4 * 1024 * 1024), tile); });
    }
    check("facility.eani", [&] {
        engine::decode_animations(assets.read("facility.eani", 1024 * 1024), "facility.eani");
    });
    check("fonts/ui.efnt", [&] { engine::load_font(assets, "fonts/ui.efnt"); });
    for (const auto key : {"sounds/pickup.wav", "sounds/music.wav", "sounds/machine.wav",
                           "sounds/door.wav"})
        check(key, [&] { engine::decode_wav(assets.read(key, engine::SoundData::max_file_bytes), key); });
    for (const auto key : {"shaders/sprite.vert", "shaders/sprite.frag"})
        check(std::string(key) + " (source only)", [&] {
            const auto source = assets.text(key, engine::Renderer::max_shader_source_bytes);
            if (source.empty() || source.find('\0') != std::string::npos)
                throw std::runtime_error("Shader source is empty or contains NUL bytes");
        });

    struct Grid { std::uint32_t columns{1}, rows{1}; };
    using Bank = std::map<std::string, Grid>;
    std::array<std::optional<Bank>, 2> banks;
    Bank textures;
    for (const auto key : {"textures/actors.etex", "textures/tiles.etex",
                           "textures/white.etex", "textures/checker.etex"})
        textures.emplace(key, Grid{});
    const auto add = [](Bank& bank, const std::string& key, std::uint32_t columns,
                        std::uint32_t rows) {
        engine::AssetRoot::validate_key(key);
        engine::validate_atlas(columns, rows);
        auto& grid = bank[key];
        // Reject before the next LCM, bounding arithmetic and matching residency.
        const auto c = std::lcm(grid.columns, columns), r = std::lcm(grid.rows, rows);
        if (c > 4096 || r > 4096)
            throw std::runtime_error("Incompatible shared atlas grids: " + key);
        grid = {c, r};
    };
    for (std::size_t i = 0; i < banks.size(); ++i) {
        const auto level = i == 0 ? Level::facility : Level::annex;
        check(std::string(level_name(level)) + " gameplay", [&] {
            auto game = load_game(assets, level);
            // Includes walk and loop-mode requirements not exercised at tick zero.
            game.reload_animations(game.animations);
            Bank bank;
            add(bank, game.map.data().texture, game.map.data().atlas_columns,
                game.map.data().atlas_rows);
            add(bank, game.animations->texture, game.animations->atlas_columns,
                game.animations->atlas_rows);
            game.world.each([&](const engine::SceneNode& node) {
                if (node.sprite)
                    add(bank, node.sprite->texture, 1, 1);
            });
            // Each level's grid is checked separately below, without imposing an
            // artificial combined atlas limit across different starting levels.
            for (const auto& [key, grid] : bank) {
                (void)grid;
                textures.try_emplace(key);
            }
            banks[i] = std::move(bank);
        });
    }
    engine::TextureCache cache(assets);
    for (const auto& [key, unused] : textures) {
        (void)unused;
        check(key, [&] {
            const auto image = cache.load(key); // Strict decode, never a fallback.
            for (const auto& bank : banks)
                if (bank) {
                    const auto found = bank->find(key);
                    if (found != bank->end() &&
                        (image->width % found->second.columns || image->height % found->second.rows))
                        throw std::runtime_error("Texture dimensions must fit every registered atlas grid");
                }
        });
    }
    if (banks[0] && banks[1]) {
        for (std::size_t from = 0; from < 2; ++from)
            check(from == 0 ? "FACILITY -> ANNEX texture residency" :
                              "ANNEX -> FACILITY texture residency", [&] {
                for (const auto& [key, grid] : *banks[1 - from]) {
                    const auto found = banks[from]->find(key);
                    if (found == banks[from]->end() || found->second.columns % grid.columns ||
                        found->second.rows % grid.rows)
                        throw std::runtime_error("Starting level lacks compatible resident texture: " + key);
                }
            });
    }
    return result;
}
} // namespace feature_lab
