#include "engine/animation.hpp"
#include "engine/assets.hpp"
#include "engine/scene.hpp"
#include "engine/sound.hpp"
#include "engine/tilemap.hpp"
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
using Pixel = std::array<std::uint8_t, 4>;
void pixel(engine::TextureData& image, int x, int y, Pixel value) {
    const auto offset =
        (static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x)) * 4;
    std::copy(value.begin(), value.end(), image.rgba.begin() + static_cast<std::ptrdiff_t>(offset));
}
engine::TextureData tile_atlas() {
    engine::TextureData image{48, 8, std::vector<std::uint8_t>(48 * 8 * 4)};
    for (int cell = 0; cell < 6; ++cell)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                Pixel color{};
                if (cell < 2) {
                    color = cell == 0 ? Pixel{35, 52, 63, 255} : Pixel{39, 57, 68, 255};
                    if (x == 0 || y == 0)
                        color = {25, 40, 51, 255};
                    if (x == 6 && y == 6)
                        color = {54, 76, 87, 255};
                } else if (cell == 2) {
                    color = {70, 103, 117, 255};
                    if (y == 7 || x == 0)
                        color = {112, 153, 161, 255};
                    if (y == 0 || x == 7)
                        color = {30, 49, 63, 255};
                    if ((x == 2 || x == 5) && (y == 2 || y == 5))
                        color = {42, 66, 80, 255};
                } else if (cell == 3) {
                    color = (x % 2 == 0) ? Pixel{48, 78, 88, 255} : Pixel{15, 28, 40, 255};
                } else if (cell == 4) {
                    if (y >= 3 && y <= 5)
                        color = y == 5 ? Pixel{91, 164, 171, 245} : Pixel{35, 75, 92, 235};
                } else {
                    color = {19, 72, 81, 255};
                    if (x == y || x + y == 7)
                        color = {35, 157, 168, 255};
                }
                pixel(image, cell * 8 + x, y, color);
            }
    return image;
}
engine::TextureData animation_atlas() {
    engine::TextureData image{128, 48, std::vector<std::uint8_t>(128 * 48 * 4)};
    for (int cell = 0; cell < 24; ++cell)
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                Pixel color{};
                if (cell <= 3) { // Robot idle + three walking poses.
                    const int bob = cell == 2 ? 1 : 0;
                    if (x >= 3 && x <= 12 && y >= 4 + bob && y <= 12 + bob)
                        color = {44, 211, 182, 255};
                    if (x >= 4 && x <= 11 && y == 12 + bob)
                        color = {134, 249, 222, 255};
                    if (x >= 7 && x <= 11 && y >= 8 + bob && y <= 10 + bob)
                        color = {12, 42, 53, 255};
                    if (x == 10 && y == 9 + bob)
                        color = {196, 255, 240, 255};
                    const int foot = cell == 1 ? 1 : cell == 3 ? -1 : 0;
                    if (((x >= 3 && x <= 5 && y >= 2 + foot && y < 4 + bob) ||
                         (x >= 10 && x <= 12 && y >= 2 - foot && y < 4 + bob)))
                        color = {19, 113, 112, 255};
                } else if (cell >= 4 && cell <= 7) {
                    const int radius = 3 + (cell == 5 || cell == 6 ? 1 : 0);
                    const int distance = std::abs(x - 7) + std::abs(y - 7);
                    if (distance <= radius)
                        color = distance < radius - 1 ? Pixel{255, 230, 146, 255}
                                                      : Pixel{244, 169, 49, 255};
                    if (cell == 6 && ((x == 1 && y == 12) || (x == 13 && y == 2)))
                        color = {255, 239, 166, 255};
                } else if (cell >= 8 && cell <= 11) {
                    if (x >= 1 && x <= 14 && y >= 1 && y <= 14)
                        color = {30, 56, 71, 255};
                    if (x >= 3 && x <= 12 && y >= 3 && y <= 12)
                        color = {11, 29, 43, 255};
                    const int frame = cell - 8;
                    if ((frame % 2 == 0 && (std::abs(x - 7) <= 1 || std::abs(y - 7) <= 1)) ||
                        (frame % 2 == 1 && (std::abs(x - y) <= 1 || std::abs(x + y - 14) <= 1))) {
                        if (x >= 4 && x <= 11 && y >= 4 && y <= 11)
                            color = {74, 130, 145, 255};
                    }
                    if (y == 13 && x >= 3 && x <= 3 + frame * 3)
                        color = {40, 227, 185, 255};
                } else if (cell >= 16 && cell <= 19) {
                    const int gap = (cell - 16) * 2;
                    if (x == 0 || x == 15 || y == 0 || y == 15)
                        color = {81, 113, 123, 255};
                    if (x > 0 && x < 15 && y > 0 && y < 15) {
                        if (x < 8 - gap || x >= 8 + gap)
                            color = x % 3 == 0 ? Pixel{81, 132, 143, 255} : Pixel{41, 88, 106, 255};
                        else
                            color = {8, 28, 35, 110};
                    }
                    if (y == 14 && x >= 5 && x <= 10)
                        color = gap == 0 ? Pixel{244, 163, 47, 255} : Pixel{52, 233, 184, 255};
                }
                pixel(image, (cell % 8) * 16 + x, (cell / 8) * 16 + y, color);
            }
    return image;
}
engine::TileMapData facility_map() {
    engine::TileMapData map;
    map.width = 64;
    map.height = 32;
    map.origin = {-32, -16};
    map.tile_size = 1;
    map.texture = "textures/tiles.etex";
    map.atlas_columns = 6;
    map.atlas_rows = 1;
    map.palette = {{0, false}, {0, false}, {1, false}, {2, true},
                   {3, false}, {4, false}, {5, false}};
    for (int order : {-10, 0, 40})
        map.layers.push_back({order, std::vector<std::uint16_t>(64 * 32)});
    const auto put = [&](std::size_t layer, int x, int y, std::uint16_t id) {
        map.layers[layer]
            .cells[static_cast<std::size_t>(y + 16) * 64 + static_cast<std::size_t>(x + 32)] = id;
    };
    for (int y = -16; y < 16; ++y)
        for (int x = -32; x < 32; ++x) {
            put(0, x, y, (x + y) % 2 == 0 ? 1 : 2);
            if (x == -32 || x == 31 || y == -16 || y == 15)
                put(1, x, y, 3);
        }
    for (int y = -14; y < -1; ++y)
        put(1, -12, y, 3);
    for (int y = 2; y < 15; ++y)
        put(1, 12, y, 3);
    for (int x = -25; x < -17; ++x)
        put(2, x, 5, 5);
    for (int x = 17; x < 26; ++x)
        put(2, x, 4, 5);
    for (int y = 6; y < 12; ++y)
        for (int x = -2; x < 3; ++x)
            put(1, x, y, 4);
    for (int x = -6; x < 7; ++x)
        put(0, x, 0, 6);
    return map;
}
engine::SoundData sound_asset(int kind) {
    const std::size_t frames = kind == 0 ? 7200U : kind == 1 ? 96000U : kind == 2 ? 48000U : 19200U;
    engine::SoundData sound{kind == 1 ? 2U : 1U, {}};
    sound.samples.resize(frames * sound.channels);
    constexpr double tau = 6.283185307179586;
    for (std::size_t i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / 48000;
        const double progress = static_cast<double>(i) / static_cast<double>(frames);
        if (kind == 1) {
            sound.samples[2 * i] =
                static_cast<float>(0.12 * std::sin(tau * 110 * t) + 0.06 * std::sin(tau * 164 * t));
            sound.samples[2 * i + 1] =
                static_cast<float>(0.12 * std::sin(tau * 110 * t) + 0.06 * std::sin(tau * 165 * t));
        } else if (kind == 2)
            sound.samples[i] = static_cast<float>(
                (0.15 * std::sin(tau * 80 * t) + 0.08 * std::sin(tau * 120 * t)) *
                (0.8 + 0.2 * std::cos(tau * 10 * t)));
        else {
            const double envelope = std::min(t / 0.005, 1.0) * (1 - progress);
            const double phase =
                kind == 0 ? tau * (660 * t + 440 * t * t) : tau * (180 * t - 100 * t * t);
            sound.samples[i] = static_cast<float>(0.6 * std::sin(phase) * envelope);
        }
    }
    return sound;
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: make_assets OUTPUT_DIRECTORY\n";
            return 1;
        }
        std::filesystem::create_directories(std::filesystem::path(argv[1]) / "textures");
        std::filesystem::create_directories(std::filesystem::path(argv[1]) / "sounds");
        engine::AssetRoot root(argv[1]);
        int sound_kind = 0;
        for (const auto key :
             {"sounds/pickup.wav", "sounds/music.wav", "sounds/machine.wav", "sounds/door.wav"})
            root.write_atomic(key, engine::encode_wav(sound_asset(sound_kind++)));
        root.write_atomic("textures/white.etex",
                          engine::encode_texture({1, 1, {255, 255, 255, 255}}));
        root.write_atomic("textures/checker.etex",
                          engine::encode_texture({2,
                                                  2,
                                                  {255, 255, 255, 255, 160, 160, 160, 255, 160, 160,
                                                   160, 255, 255, 255, 255, 255}}));
        root.write_atomic("textures/tiles.etex", engine::encode_texture(tile_atlas()));
        root.write_atomic("textures/actors.etex", engine::encode_texture(animation_atlas()));
        root.write_atomic("facility.etmp", engine::encode_tilemap(facility_map()));
        engine::AnimationSet animations{
            "textures/actors.etex",
            8,
            3,
            {{"idle", true, {{0, 60}}},
             {"walk", true, {{1, 6}, {2, 6}, {3, 6}, {2, 6}}},
             {"core_pulse", true, {{4, 8}, {5, 8}, {6, 8}, {7, 8}}},
             {"machine", true, {{8, 5}, {9, 5}, {10, 5}, {11, 5}}},
             {"door_open", false, {{16, 8}, {17, 8}, {18, 8}, {19, 8}}}}};
        root.write_atomic("facility.eani", engine::encode_animations(animations));
        engine::Scene scene;
        const auto group = scene.create(1, "Facility", "group");
        const auto add = [&](std::uint64_t id, const char* name, const char* tag, engine::Vec2 p,
                             engine::Vec2 size, int layer) {
            const auto e = scene.create(id, name, tag);
            scene.set_parent(e, group);
            scene.set_transform(e, {p, 0, {1, 1}});
            scene.set_sprite(e, {animations.texture, size, 1, 1, 1, 1, layer});
            return e;
        };
        add(20, "Player", "player", {-27, -10}, {0.9F, 0.95F}, 30);
        add(21, "Exit door", "exit", {28, -10}, {2.4F, 1.8F}, 10);
        std::uint64_t id = 30;
        for (auto p : {engine::Vec2{-22, 8}, engine::Vec2{0, -9}, engine::Vec2{22, 8}})
            add(id++, "Core", "core", p, {0.9F, 0.9F}, 20);
        const auto machine = add(40, "Ventilation", "machine", {0, 9}, {2.5F, 2.5F}, 10);
        scene.set_collider(machine, {{1, 1}});
        root.write_text("facility.scene", engine::serialize_scene(scene));
        std::cout << "Wrote " << scene.size()
                  << " entities, 64x32 three-layer map, five animation clips, atlases and four WAV "
                     "sounds to "
                  << root.directory() << '\n';
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
