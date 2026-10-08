#pragma once
#include "engine/collision_world.hpp"
#include "engine/mixer.hpp"
#include "engine/random.hpp"
#include "engine/scene.hpp"
#include "engine/tilemap.hpp"
#include <bit>

namespace feature_lab {
struct StressConfig {
    static constexpr std::size_t max_sprites = 65536, max_tiles = 65536;
    std::size_t sprites{8192}, tiles{4096}, entities{1024}, bodies{128}, voices{8};
    std::uint64_t seed{1};
    bool dense{};
    void validate() const {
        if (sprites > max_sprites || tiles > max_tiles || entities > engine::Scene::capacity ||
            bodies > engine::CollisionWorld::max_bodies || voices > engine::Mixer::max_voices)
            throw std::invalid_argument("Stress workload exceeds an engine/arena capacity");
    }
};
struct StressSprite {
    engine::Vec2 initial, position;
    std::uint32_t cell{};
};
struct StressResources {
    std::size_t sprites{}, entities{}, bodies{}, tiles{}, voices{}, buffer_bytes{};
    bool operator==(const StressResources&) const = default;
};
// CPU workloads own all storage. No window, driver, audio device or authored assets required.
class StressArena {
  public:
    explicit StressArena(StressConfig config)
        : config_(checked(config)), tiles_(make_tiles(config_)) {
        engine::Random random(config.seed);
        sprites_.reserve(config.sprites);
        for (std::size_t i = 0; i < config.sprites; ++i) {
            const auto coordinate = [&] {
                return static_cast<float>(random.next() % 30001) / 1000.0F - 15.0F;
            };
            engine::Vec2 position{coordinate(), coordinate()};
            if (i % 4 == 0)
                position.x += 100; // Deliberately offscreen; exercises renderer culling.
            sprites_.push_back({position, position, static_cast<std::uint32_t>(i % 4)});
        }
        entities_.reserve(config.entities);
        for (std::size_t i = 0; i < config.entities; ++i)
            entities_.push_back(scene_.create(i + 1, "stress"));
        bodies_.reserve(config.bodies);
        for (std::size_t i = 0; i < config.bodies; ++i)
            bodies_.push_back({i + 1, engine::Shape2D::circle({}, 0.6F), 1, 1, true, true});
        for (std::size_t i = 0; i < sound_.size(); ++i)
            sound_[i] = (static_cast<float>(i < 32 ? i : 64 - i) / 16 - 1) * 0.1F;
        for (std::size_t i = 0; i < config.voices; ++i) {
            const float pan =
                config.voices <= 1
                    ? 0
                    : 2 * static_cast<float>(i) / static_cast<float>(config.voices - 1) - 1;
            if (!mixer_.play(engine::SoundView{sound_, 1}, {0.1F, pan, true}))
                throw std::runtime_error("Stress voice creation failed");
        }
        update_poses();
    }
    StressArena(const StressArena&) = delete;
    StressArena& operator=(const StressArena&) = delete;
    // Mixer views point into this object; do not move it.
    StressArena(StressArena&&) = delete;
    StressArena& operator=(StressArena&&) = delete;
    void simulate() {
        ++ticks_;
        update_poses();
    }
    void mix() {
        if (config_.voices)
            mixer_.mix(output_); // Exactly 800 stereo frames: one 60 Hz simulation step.
    }
    const auto& sprites() const { return sprites_; }
    const auto& tiles() const { return tiles_; }
    const auto& scene() const { return scene_; }
    const auto& collisions() const { return collisions_; }
    std::uint64_t ticks() const { return ticks_; }
    StressResources resources() const {
        return {sprites_.size(),
                scene_.size(),
                bodies_.size(),
                config_.tiles,
                mixer_.active_voices(),
                sprites_.capacity() * sizeof(StressSprite) +
                    entities_.capacity() * sizeof(engine::Entity) +
                    bodies_.capacity() * sizeof(engine::Body2D) + tiles_.buffer_bytes() +
                    scene_.buffer_bytes() + collisions_.buffer_bytes() + sizeof(sound_) +
                    sizeof(output_) + sizeof(mixer_)};
    }
    std::uint64_t checksum() const {
        std::uint64_t hash = 14695981039346656037ULL;
        const auto add = [&](std::uint64_t value) {
            for (unsigned i = 0; i < 8; ++i)
                hash = (hash ^ static_cast<std::uint8_t>(value >> (i * 8))) * 1099511628211ULL;
        };
        const auto point = [&](engine::Vec2 p) {
            add(std::bit_cast<std::uint32_t>(p.x));
            add(std::bit_cast<std::uint32_t>(p.y));
        };
        add(ticks_);
        add(config_.seed);
        for (const auto& sprite : sprites_) {
            point(sprite.position);
            add(sprite.cell);
        }
        scene_.each([&](const auto& node) {
            add(node.id);
            point(node.local.position);
        });
        for (const auto& body : collisions_.bodies()) {
            add(body.id);
            point(body.shape.center);
        }
        for (const auto& contact : collisions_.contacts()) {
            add(contact.a);
            add(contact.b);
        }
        for (const auto cell : tiles_.data().layers[0].cells)
            add(cell);
        for (const auto value : output_)
            add(std::bit_cast<std::uint32_t>(value));
        add(mixer_.active_voices());
        return hash;
    }

  private:
    static StressConfig checked(StressConfig config) {
        config.validate();
        return config;
    }
    static engine::TileMapData make_tiles(const StressConfig& config) {
        engine::TileMapData data{256,
                                 256,
                                 {-16, -16},
                                 0.125F,
                                 "stress.etex",
                                 2,
                                 2,
                                 {{0, false}, {0, false}, {1, false}, {2, false}, {3, false}},
                                 {{0, std::vector<std::uint16_t>(65536)}}};
        // Odd multiplier is a permutation modulo 65536: exact occupancy at every count.
        for (std::size_t i = 0; i < config.tiles; ++i)
            data.layers[0]
                .cells[(i * 40503 + static_cast<std::size_t>(config.seed & 65535)) & 65535] =
                static_cast<std::uint16_t>(1 + i % 4);
        return data;
    }
    void update_poses() {
        const auto phase = ticks_ % 120;
        const float offset = static_cast<float>(phase < 60 ? phase : 120 - phase) / 120;
        for (auto& sprite : sprites_)
            sprite.position = sprite.initial + engine::Vec2{offset, -offset};
        for (std::size_t i = 0; i < entities_.size(); ++i)
            scene_.set_transform(
                entities_[i],
                {{static_cast<float>(i % 64) + offset, static_cast<float>(i / 64)}, 0, {1, 1}});
        for (std::size_t i = 0; i < bodies_.size(); ++i) {
            const float spacing = config_.dense ? 0.02F : 2.0F;
            bodies_[i].shape.center = {static_cast<float>(i % 16) * spacing + offset,
                                       static_cast<float>(i / 16) * spacing};
        }
        collisions_.update(bodies_);
    }
    StressConfig config_;
    engine::TileMap tiles_;
    engine::Scene scene_;
    engine::CollisionWorld collisions_;
    std::vector<StressSprite> sprites_;
    std::vector<engine::Entity> entities_;
    std::vector<engine::Body2D> bodies_;
    std::array<float, 64> sound_{};
    std::array<float, 1600> output_{};
    engine::Mixer mixer_;
    std::uint64_t ticks_{};
};
} // namespace feature_lab
