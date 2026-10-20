#pragma once
#include "decorations.hpp"
#include "engine/animation.hpp"
#include "engine/assets.hpp"
#include "engine/collision.hpp"
#include "engine/collision_world.hpp"
#include "engine/input.hpp"
#include "engine/persistence.hpp"
#include "engine/random.hpp"
#include "engine/scene.hpp"
#include "engine/staging_memory.hpp"
#include "engine/tilemap.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace feature_lab {
using engine::Vec2;
enum class Level { facility, annex };
inline std::string_view level_name(Level level) {
    return level == Level::facility ? "FACILITY" : "ANNEX";
}
inline Level next_level(Level level) {
    return level == Level::facility ? Level::annex : Level::facility;
}
struct Checkpoint {
    std::array<std::uint32_t, 3> content{};
    Vec2 position{};
    float camera_height{14};
    std::uint64_t ticks{};
    bool paused{}, won{}, door_started{}, door_open{};
    std::vector<std::uint64_t> collected_ids;
    std::array<engine::AnimationState, 4> animations;
    std::uint64_t alarm_entries{};
    bool alarm_disabled{};
    std::uint64_t random_seed{1}, random_state{1};
};
// Runtime data rebuilt from authored core tags; the existing save format stores
// collected persistent IDs. Destruction removes this component with its entity.
struct CorePickup {
    std::size_t slot{};
    float radius{0.65F};
};
struct Game {
    // Only player/alarm participate in reciprocal detection; walls remain queryable.
    // Leave headroom above the current one-pair demand without shrinking body capacity.
    static constexpr engine::CollisionWorld::Limits collision_limits{16, 32};
    Level level{Level::facility};
    Vec2 spawn_position{};
    engine::TileMap map;
    std::shared_ptr<const engine::AnimationSet> animations;
    engine::AnimationPlayer player_animation, core_animation, machine_animation, door_animation;
    engine::Scene world;
    Vec2 position{}, previous{};
    std::vector<engine::Rect> walls, collision_candidates;
    std::vector<engine::Entity> keys, draw_order;
    engine::Entity player, exit;
    std::vector<bool> collected;
    std::vector<std::uint64_t> core_ids;
    std::uint64_t ticks{};
    std::uint64_t previous_ticks{}; // Transient presentation state; snapped on load/step/restart.
    bool paused{}, won{}, door_started{}, door_open{};
    std::uint64_t random_seed{1};
    engine::Random random;
    unsigned door_completions{};
    float camera_height{14};
    engine::CollisionWorld collisions{collision_limits};
    std::uint64_t alarm_entries{};
    bool alarm_disabled{}, alarm_touching{};
    static constexpr Vec2 switch_position{-27, -7.5F};
    Vec2 switch_location() const {
        return level == Level::facility ? switch_position : Vec2{-15, 3};
    }
    Vec2 alarm_position() const {
        const auto phase = ticks % 240;
        const auto distance = phase <= 120 ? phase : 240 - phase;
        return (level == Level::facility ? Vec2{-24, -12} : Vec2{-12, -6}) +
               Vec2{0, static_cast<float>(distance) / 30};
    }
    Game(std::string scene_text, engine::TileMapData tiles,
         std::shared_ptr<const engine::AnimationSet> clips, Level selected = Level::facility)
        : level(selected), map(std::move(tiles)), animations(std::move(clips)),
          player_animation(animations), core_animation(animations), machine_animation(animations),
          door_animation(animations), initial_(std::move(scene_text)) {
        engine::StagingBytes construction_memory(staging_buffer_bytes());
        restart(&construction_memory);
    }
    void append_obstacles(engine::Rect query, std::vector<engine::Rect>& output) const {
        map.append_colliders(query, output);
        for (const auto& wall : walls)
            if (engine::overlaps(query, wall))
                output.push_back(wall);
        const auto goal = location(exit);
        const engine::Rect door{goal - Vec2{1.2F, 0.7F}, goal + Vec2{1.2F, 0.7F}};
        if (!door_open && engine::overlaps(query, door))
            output.push_back(door);
    }
    engine::Camera camera(int width, int height, float alpha) const {
        engine::Camera result{engine::lerp(previous, position, alpha), camera_height};
        const auto half = result.extent(width, height) * 0.5F;
        const auto bounds = map.bounds();
        const auto clamp_axis = [](float p, float low, float high, float extent) {
            return high - low <= 2 * extent ? (low + high) * 0.5F
                                            : std::clamp(p, low + extent, high - extent);
        };
        result.center = {clamp_axis(result.center.x, bounds.min.x, bounds.max.x, half.x),
                         clamp_axis(result.center.y, bounds.min.y, bounds.max.y, half.y)};
        return result;
    }
    int count() const {
        return static_cast<int>(std::count(collected.begin(), collected.end(), true));
    }
    int total() const { return static_cast<int>(keys.size()); }
    // Selected owned buffers, matching the engine's existing buffer_bytes metrics.
    // Nested strings/hash nodes are outside this count; parsers track their scratch separately.
    std::size_t staging_buffer_bytes() const {
        auto bytes = map.buffer_bytes() + world.buffer_bytes() + collisions.buffer_bytes() +
            (walls.capacity() + collision_candidates.capacity() + sensor_walls_.capacity()) *
                sizeof(engine::Rect) +
            (keys.capacity() + draw_order.capacity()) * sizeof(engine::Entity) +
            (core_ids.capacity() + sensor_query_.capacity()) * sizeof(std::uint64_t) +
            sensor_bodies_.capacity() * sizeof(engine::Body2D) + collected.capacity() / 8 +
            initial_.size() + animations->buffer_bytes();
        return bytes;
    }
    Vec2 location(engine::Entity entity) const {
        const auto t = world.world_transform(entity);
        return {t.x, t.y};
    }
    engine::Transform render_transform(engine::Entity entity, float alpha) const {
        if (!std::isfinite(alpha) || alpha < 0 || alpha > 1)
            throw std::invalid_argument("Render interpolation must be in [0,1]");
        float phase = static_cast<float>(previous_ticks % 240) +
                      static_cast<float>(ticks - previous_ticks) * alpha;
        if (phase >= 240)
            phase -= 240; // Canonical wrap: live and restored integer-tick poses are identical.
        return world.sampled_world_transform(entity, [&](const engine::SceneNode& node) {
            auto pose = decoration_pose(node, phase);
            if (node.entity == player)
                pose.position = engine::lerp(previous, position, alpha);
            return pose;
        });
    }
    int update(const engine::InputFrame& input) {
        if (engine::button(input, engine::Key::restart).pressed) {
            restart();
            return 0;
        }
        const bool toggle_pause = engine::button(input, engine::Key::pause).pressed ||
                                  engine::button(input, engine::Key::space).pressed;
        const bool single_step =
            paused && !toggle_pause && engine::button(input, engine::Key::single_step).pressed;
        if (toggle_pause)
            paused = !paused;
        previous = position;
        previous_ticks = ticks;
        if ((paused && !single_step) || won)
            return 0;
        ++ticks;
        if (ticks % 60 == 0)
            random.next();
        camera_height = std::clamp(
            camera_height + (static_cast<float>(engine::button(input, engine::Key::zoom_out).held) -
                             static_cast<float>(engine::button(input, engine::Key::zoom_in).held)) *
                                0.15F,
            8.0F, 26.0F);
        Vec2 direction{static_cast<float>(engine::button(input, engine::Key::right).held) -
                           static_cast<float>(engine::button(input, engine::Key::left).held),
                       static_cast<float>(engine::button(input, engine::Key::up).held) -
                           static_cast<float>(engine::button(input, engine::Key::down).held)};
        const auto delta = engine::normalized(direction) * (5.0F / 60.0F);
        const auto next = position + delta;
        collision_candidates.clear();
        append_obstacles(
            {{std::min(position.x, next.x) - 0.3F, std::min(position.y, next.y) - 0.3F},
             {std::max(position.x, next.x) + 0.3F, std::max(position.y, next.y) + 0.3F}},
            collision_candidates);
        position = engine::move_box(position, {0.3F, 0.3F}, delta, collision_candidates);
        player_animation.play(
            position.x != previous.x || position.y != previous.y ? "walk" : "idle", false);
        player_animation.advance(1);
        core_animation.advance(1);
        machine_animation.advance(1);
        auto transform = world.get(player).local;
        transform.position = position;
        world.set_transform(player, transform);
        int picked = 0;
        world.each_component<CorePickup>([&](engine::Entity entity, const CorePickup& core) {
            const auto p = location(entity);
            if (std::hypot(position.x - p.x, position.y - p.y) < core.radius) {
                collected[core.slot] = true;
                ++picked;
                world.defer_destroy(entity);
            }
        });
        world.flush();
        if (count() == total() && !door_started) {
            door_started = true;
            door_animation.play("door_open");
        }
        if (door_animation.advance(1).completed) {
            door_open = true;
            ++door_completions;
        }
        update_sensors(false);
        if (engine::button(input, engine::Key::interact).pressed)
            interact_switch();
        const auto goal = location(exit);
        if (door_open && std::hypot(position.x - goal.x, position.y - goal.y) < 0.8F)
            won = true;
        if (paused)
            previous = position; // A stepped state must remain visible at any render alpha.
        if (paused || won)
            previous_ticks = ticks;
        return picked;
    }

    // Main-thread frame boundary. Stage every player before publishing the bank.
    void reload_animations(std::shared_ptr<const engine::AnimationSet> candidate) {
        if (!candidate)
            throw std::invalid_argument("Missing animation candidate");
        candidate->validate();
        if (candidate->texture != animations->texture ||
            candidate->atlas_columns != animations->atlas_columns ||
            candidate->atlas_rows != animations->atlas_rows)
            throw std::runtime_error("Animation reload must retain the resident texture/atlas");
        for (const auto name : {"idle", "walk", "core_pulse", "machine", "door_open"}) {
            const auto clip = std::find_if(candidate->clips.begin(), candidate->clips.end(),
                                           [&](const auto& c) { return c.name == name; });
            if (clip == candidate->clips.end() ||
                clip->loop != (std::string_view(name) != "door_open"))
                throw std::runtime_error("Missing or incompatible required animation: " +
                                         std::string(name));
        }
        auto player_next = player_animation, core_next = core_animation,
             machine_next = machine_animation, door_next = door_animation;
        player_next.replace(candidate);
        core_next.replace(candidate);
        machine_next.replace(candidate);
        door_next.replace(candidate);
        static_assert(std::is_nothrow_move_assignable_v<engine::AnimationPlayer>);
        animations = std::move(candidate);
        player_animation = std::move(player_next);
        core_animation = std::move(core_next);
        machine_animation = std::move(machine_next);
        door_animation = std::move(door_next);
    }

    // Stage collision/query storage and dependent presentation before publication.
    // Keep the authored scene's coordinate system and resident atlas unchanged.
    void reload_tilemap(engine::TileMapData data,
                        const std::function<void(const engine::TileMap&)>& prepare = {}) {
        const auto& old = map.data();
        if (data.width != old.width || data.height != old.height || data.origin.x != old.origin.x ||
            data.origin.y != old.origin.y || data.tile_size != old.tile_size ||
            data.texture != old.texture || data.atlas_columns != old.atlas_columns ||
            data.atlas_rows != old.atlas_rows)
            throw std::runtime_error("Tilemap reload must retain grid geometry and texture/atlas");
        engine::TileMap candidate(std::move(data));
        if (candidate.solid_cells() + walls.size() + 4 > engine::CollisionWorld::max_bodies)
            throw std::runtime_error("Tilemap reload exceeds collision body capacity");
        std::vector<engine::Rect> next_walls;
        next_walls.reserve(candidate.solid_cells() + walls.size());
        candidate.append_colliders(candidate.bounds(), next_walls);
        next_walls.insert(next_walls.end(), walls.begin(), walls.end());
        const auto validate_position = [&](Vec2 center, Vec2 half) {
            const engine::Rect box{center - half, center + half};
            const auto bounds = candidate.bounds();
            if (box.min.x < bounds.min.x || box.min.y < bounds.min.y || box.max.x > bounds.max.x ||
                box.max.y > bounds.max.y)
                throw std::runtime_error("Tilemap reload excludes player or restart spawn");
            for (const auto& wall : next_walls)
                if (engine::overlaps(box, wall))
                    throw std::runtime_error("Tilemap reload blocks player or restart spawn");
        };
        // Match save validation's tolerance at swept-collision boundaries.
        validate_position(position, {0.2999F, 0.2999F});
        validate_position(spawn_position, {0.3F, 0.3F});
        std::vector<engine::Body2D> next_bodies;
        next_bodies.reserve(engine::CollisionWorld::max_bodies);
        stage_sensor_bodies(next_walls, next_bodies);
        engine::CollisionWorld next_collisions(collisions.limits());
        next_collisions.prime(next_bodies); // Seed current contacts without synthetic enters.
        std::vector<engine::Rect> next_candidates;
        next_candidates.reserve(next_walls.size() + 1);
        if (prepare)
            prepare(candidate);
        static_assert(std::is_nothrow_move_assignable_v<engine::TileMap>);
        static_assert(std::is_nothrow_move_assignable_v<engine::CollisionWorld>);
        map = std::move(candidate);
        sensor_walls_ = std::move(next_walls);
        sensor_bodies_ = std::move(next_bodies);
        collisions = std::move(next_collisions);
        collision_candidates = std::move(next_candidates);
        previous = position; // Do not interpolate through newly solid tiles.
    }

    // Rebuild runtime bindings by persistent ID in a private world. Successful
    // publication invalidates every old entity handle; failure retains them all.
    void reload_scene(std::string text, const std::function<void(const Game&)>& prepare = {}) {
        Game candidate(std::move(text), map.data(), animations, level);
        auto ids = core_ids, next_ids = candidate.core_ids;
        std::sort(ids.begin(), ids.end());
        std::sort(next_ids.begin(), next_ids.end());
        if (ids != next_ids || world.get(player).id != candidate.world.get(candidate.player).id ||
            world.get(exit).id != candidate.world.get(candidate.exit).id)
            throw std::runtime_error("Scene reload must retain player, exit and core IDs/roles");
        candidate.world.each([&](const engine::SceneNode& node) {
            if (node.collider && (node.tag == "player" || node.tag == "core" || node.tag == "exit"))
                throw std::runtime_error("Gameplay actors cannot own static colliders");
            for (auto p = node.parent; p; p = candidate.world.get(p).parent) {
                const auto& parent = candidate.world.get(p);
                if (parent.tag == "player" || parent.tag == "core")
                    throw std::runtime_error("Player and core nodes must remain leaves");
            }
        });
        const auto state = checkpoint();
        candidate.apply_checkpoint(state); // Validates live occupancy and door/win consistency.
        candidate.door_completions = door_completions;
        if (prepare)
            prepare(candidate); // Validate resident resources and stage dependent views.
        replace(std::move(candidate));
    }

    std::array<std::uint32_t, 3> content_id() const {
        return {engine::crc32(
                    {reinterpret_cast<const std::uint8_t*>(initial_.data()), initial_.size()}),
                engine::crc32(engine::encode_tilemap(map.data())),
                engine::crc32(engine::encode_animations(*animations))};
    }
    Checkpoint checkpoint() const {
        Checkpoint state{content_id(),
                         position,
                         camera_height,
                         ticks,
                         paused,
                         won,
                         door_started,
                         door_open,
                         {},
                         {player_animation.snapshot(), core_animation.snapshot(),
                          machine_animation.snapshot(), door_animation.snapshot()}};
        state.random_seed = random_seed;
        state.random_state = random.state();
        state.alarm_entries = alarm_entries;
        state.alarm_disabled = alarm_disabled;
        for (std::size_t i = 0; i < collected.size(); ++i)
            if (collected[i])
                state.collected_ids.push_back(core_ids[i]);
        return state;
    }
    void validate_checkpoint(const Checkpoint& state) const {
        if (state.content != content_id())
            throw std::runtime_error("Checkpoint content differs from installed facility");
        Game candidate(initial_, map.data(), animations, level);
        candidate.apply_checkpoint(state);
    }
    void restore(const Checkpoint& state, const std::function<void(const Game&)>& prepare = {}) {
        if (state.content != content_id())
            throw std::runtime_error("Checkpoint content differs from installed facility");
        Game candidate(initial_, map.data(), animations, level);
        candidate.apply_checkpoint(state);
        if (prepare)
            prepare(candidate);
        replace(std::move(candidate));
    }
    // The scene checks both worlds for active iteration before any publication.
    // Other owned containers/resources have noexcept move assignment.
    void replace(Game&& candidate) {
        if (&candidate == this)
            throw std::invalid_argument("Cannot replace a game with itself");
        static_assert(std::is_nothrow_move_assignable_v<engine::TileMap>);
        static_assert(std::is_nothrow_move_assignable_v<engine::CollisionWorld>);
        static_assert(std::is_nothrow_move_assignable_v<engine::AnimationPlayer>);
        // Commit world first: its move validates scene iteration before changing anything.
        // All following moves use standard allocators and cannot fail.
        world = std::move(candidate.world);
        map = std::move(candidate.map);
        animations = std::move(candidate.animations);
        initial_ = std::move(candidate.initial_);
        level = candidate.level;
        spawn_position = candidate.spawn_position;
        position = previous = candidate.position;
        camera_height = candidate.camera_height;
        ticks = candidate.ticks;
        previous_ticks = ticks;
        random_seed = candidate.random_seed;
        random = candidate.random;
        paused = candidate.paused;
        won = candidate.won;
        door_started = candidate.door_started;
        door_open = candidate.door_open;
        door_completions = candidate.door_completions;
        collisions = std::move(candidate.collisions);
        sensor_bodies_ = std::move(candidate.sensor_bodies_);
        sensor_walls_ = std::move(candidate.sensor_walls_);
        sensor_query_ = std::move(candidate.sensor_query_);
        alarm_entries = candidate.alarm_entries;
        alarm_disabled = candidate.alarm_disabled;
        alarm_touching = candidate.alarm_touching;
        keys = std::move(candidate.keys);
        core_ids = std::move(candidate.core_ids);
        collected = std::move(candidate.collected);
        draw_order = std::move(candidate.draw_order);
        walls = std::move(candidate.walls);
        collision_candidates = std::move(candidate.collision_candidates);
        player = candidate.player;
        exit = candidate.exit;
        player_animation = std::move(candidate.player_animation);
        core_animation = std::move(candidate.core_animation);
        machine_animation = std::move(candidate.machine_animation);
        door_animation = std::move(candidate.door_animation);
    }

  private:
    std::string initial_;
    std::vector<engine::Body2D> sensor_bodies_;
    std::vector<engine::Rect> sensor_walls_;
    std::vector<std::uint64_t> sensor_query_;
    void stage_sensor_bodies(const std::vector<engine::Rect>& static_walls,
                             std::vector<engine::Body2D>& bodies) const {
        using namespace engine;
        bodies.clear();
        bodies.push_back({1, Shape2D::box(position, {0.3F, 0.3F}), 1, 2, true, false});
        bodies.push_back(
            {2, Shape2D::circle(alarm_position(), 0.7F), 2, alarm_disabled ? 0U : 1U, true, true});
        bodies.push_back({3, Shape2D::box(switch_location(), {0.4F, 0.4F}), 4, 0, false, true});
        for (std::size_t i = 0; i < static_walls.size(); ++i) {
            const auto r = static_walls[i];
            bodies.push_back({1000 + i,
                              Shape2D::box((r.min + r.max) * 0.5F, (r.max - r.min) * 0.5F), 8, 0,
                              false, false});
        }
        if (!door_open)
            bodies.push_back({4, Shape2D::box(location(exit), {1.2F, 0.7F}), 8, 0, false, false});
    }
    void update_sensors(bool prime) {
        using namespace engine;
        stage_sensor_bodies(sensor_walls_, sensor_bodies_);
        if (prime)
            collisions.prime(sensor_bodies_);
        else
            collisions.update(sensor_bodies_);
        collisions.query(Shape2D::box(position, {0.3F, 0.3F}), sensor_query_,
                         {alarm_disabled ? 0U : 2U, 1, true});
        alarm_touching = !sensor_query_.empty();
        for (const auto& event : collisions.events())
            if (event.a == 1 && event.b == 2 && event.phase == TriggerPhase::enter)
                ++alarm_entries;
    }
    void interact_switch() {
        const auto d = switch_location() - position;
        if (std::hypot(d.x, d.y) > 3)
            return;
        const auto hit = collisions.segment(position, switch_location(), {4 | 8, 1, true});
        if (hit && hit->id == 3) {
            alarm_disabled = !alarm_disabled;
            update_sensors(false); // Mask change generates an exit/enter on this same tick.
        }
    }
    void apply_checkpoint(const Checkpoint& state) {
        const auto bounds = map.bounds();
        if (!std::isfinite(state.position.x) || !std::isfinite(state.position.y) ||
            state.position.x < bounds.min.x || state.position.x > bounds.max.x ||
            state.position.y < bounds.min.y || state.position.y > bounds.max.y ||
            !std::isfinite(state.camera_height) || state.camera_height < 8 ||
            state.camera_height > 26 || state.ticks > static_cast<std::uint64_t>(INT64_MAX) ||
            state.collected_ids.size() > keys.size() || state.alarm_entries > state.ticks * 2)
            throw std::runtime_error("Invalid checkpoint position, clock, camera or core count");
        for (const auto id : state.collected_ids) {
            const auto found = std::find(core_ids.begin(), core_ids.end(), id);
            if (found == core_ids.end())
                throw std::runtime_error("Unknown saved core ID");
            const auto index = static_cast<std::size_t>(found - core_ids.begin());
            if (collected[index])
                throw std::runtime_error("Duplicate saved core ID");
            collected[index] = true;
            world.destroy(keys[index]);
        }
        if ((state.animations[0].clip != "idle" && state.animations[0].clip != "walk") ||
            state.animations[1].clip != "core_pulse" || state.animations[2].clip != "machine" ||
            state.animations[3].clip != "door_open" || state.animations[0].paused ||
            state.animations[1].paused || state.animations[2].paused)
            throw std::runtime_error("Invalid checkpoint animation binding");
        player_animation.restore(state.animations[0]);
        core_animation.restore(state.animations[1]);
        machine_animation.restore(state.animations[2]);
        door_animation.restore(state.animations[3]);
        if (state.door_started != (count() == total()) ||
            state.door_open != (state.door_started && door_animation.finished()) ||
            state.animations[3].paused == state.door_started ||
            (!state.door_started && state.animations[3].position != 0))
            throw std::runtime_error("Inconsistent checkpoint door state");
        position = previous = state.position;
        door_open = state.door_open;
        const auto goal = location(exit);
        if (state.won != (door_open && std::hypot(position.x - goal.x, position.y - goal.y) < 0.8F))
            throw std::runtime_error("Inconsistent checkpoint completion state");
        collision_candidates.clear();
        const engine::Rect box{position - Vec2{0.2999F, 0.2999F},
                               position + Vec2{0.2999F, 0.2999F}};
        append_obstacles(box, collision_candidates);
        for (const auto& wall : collision_candidates)
            if (engine::overlaps(box, wall))
                throw std::runtime_error("Checkpoint player intersects a wall");
        auto transform = world.get(player).local;
        transform.position = position;
        world.set_transform(player, transform);
        camera_height = state.camera_height;
        ticks = state.ticks;
        random_seed = state.random_seed;
        random.restore(state.random_state);
        paused = state.paused;
        won = state.won;
        door_started = state.door_started;
        door_completions = door_open ? 1U : 0U;
        alarm_entries = state.alarm_entries;
        alarm_disabled = state.alarm_disabled;
        update_sensors(true);
    }
    void restart(engine::StagingBytes* construction_memory = nullptr) {
        const auto account = [&] {
            if (construction_memory)
                construction_memory->set(staging_buffer_bytes());
        };
        world = engine::parse_scene(initial_,
                                    level == Level::facility ? "facility.scene" : "annex.scene");
        account();
        validate_decorations(world);
        walls.clear();
        keys.clear();
        core_ids.clear();
        draw_order.clear();
        player = {};
        exit = {};
        world.each([&](const engine::SceneNode& node) {
            if (node.sprite) {
                draw_order.push_back(node.entity);
                account();
                if ((node.tag == "player" || node.tag == "exit" || node.tag == "core" ||
                     node.tag == "machine") &&
                    node.sprite->texture != animations->texture)
                    throw std::runtime_error("Animated sprite must use the animation atlas");
            }
            if (node.tag == "player") {
                if (player || !node.sprite)
                    throw std::runtime_error("Feature Lab needs one player with a sprite");
                player = node.entity;
            } else if (node.tag == "exit") {
                if (exit || !node.sprite)
                    throw std::runtime_error("Feature Lab needs one exit with a sprite");
                exit = node.entity;
            } else if (node.tag == "core") {
                if (!node.sprite)
                    throw std::runtime_error("Core is missing its sprite");
                keys.push_back(node.entity);
                account();
                core_ids.push_back(node.id);
                account();
            }
            if (node.collider) {
                const auto t = world.world_transform(node.entity);
                if (std::abs(t.b) > 1e-5F || std::abs(t.c) > 1e-5F)
                    throw std::runtime_error(
                        "Feature Lab supports axis-aligned static colliders only");
                const auto half = node.collider->half;
                const auto a = t.apply(half), b = t.apply(half * -1);
                walls.push_back({{std::min(a.x, b.x), std::min(a.y, b.y)},
                                 {std::max(a.x, b.x), std::max(a.y, b.y)}});
                account();
            }
        });
        if (!player || !exit || keys.empty() || keys.size() > 64)
            throw std::runtime_error("Feature Lab requires player, exit and 1..64 cores");
        for (std::size_t i = 0; i < keys.size(); ++i) {
            world.add_component<CorePickup>(keys[i], i);
            account();
        }
        if (const auto parent = world.get(player).parent) {
            const auto t = world.world_transform(parent);
            if (t.a != 1 || t.d != 1 || t.b != 0 || t.c != 0 || t.x != 0 || t.y != 0)
                throw std::runtime_error(
                    "Player parent must have identity world transform in this example");
        }
        std::sort(draw_order.begin(), draw_order.end(), [&](auto a, auto b) {
            const auto& x = world.get(a);
            const auto& y = world.get(b);
            return x.sprite->layer == y.sprite->layer ? x.id < y.id
                                                      : x.sprite->layer < y.sprite->layer;
        });
        position = location(player);
        spawn_position = position;
        const auto bounds = map.bounds();
        if (position.x - 0.3F < bounds.min.x || position.y - 0.3F < bounds.min.y ||
            position.x + 0.3F > bounds.max.x || position.y + 0.3F > bounds.max.y)
            throw std::runtime_error("Player spawn is outside the map");
        previous = position;
        door_started = door_open = false;
        door_completions = 0;
        camera_height = 14;
        player_animation.play("idle");
        core_animation.play("core_pulse");
        machine_animation.play("machine");
        door_animation.play("door_open");
        door_animation.pause(true);
        collision_candidates.clear();
        collision_candidates.reserve(map.solid_cells() + walls.size() + 1);
        account();
        append_obstacles(map.bounds(), collision_candidates);
        for (const auto& wall : collision_candidates)
            if (engine::overlaps({position - Vec2{0.3F, 0.3F}, position + Vec2{0.3F, 0.3F}}, wall))
                throw std::runtime_error("Player starts inside a static collider");
        collected.assign(keys.size(), false);
        account();
        ticks = 0;
        previous_ticks = 0;
        random.restore(random_seed);
        paused = false;
        won = false;
        alarm_entries = 0;
        alarm_disabled = false;
        sensor_walls_.clear();
        map.append_colliders(map.bounds(), sensor_walls_);
        account();
        sensor_walls_.insert(sensor_walls_.end(), walls.begin(), walls.end());
        account();
        if (sensor_walls_.size() + 4 > engine::CollisionWorld::max_bodies)
            throw std::runtime_error("Feature Lab collision station exceeds body capacity");
        sensor_bodies_.reserve(engine::CollisionWorld::max_bodies);
        account();
        sensor_query_.reserve(engine::CollisionWorld::max_bodies);
        account();
        update_sensors(true);
        account();
    }
};
inline Game load_game(const engine::AssetRoot& assets, Level level = Level::facility) {
    const auto stem = level == Level::facility ? "facility" : "annex";
    // Fixed loading order, with encoded files released after decoding. Each
    // returned buffer transfers its accounting from decoder to loader to Game.
    auto scene = assets.text(std::string(stem) + ".scene");
    engine::StagingBytes scene_memory(scene.size());
    engine::TileMapData tiles;
    engine::StagingBytes tile_memory;
    {
        const auto encoded = assets.read(std::string(stem) + ".etmp", 4 * 1024 * 1024);
        const engine::StagingBytes encoded_memory(encoded.capacity());
        tiles = engine::decode_tilemap(encoded, stem);
        tile_memory.set(tiles.buffer_bytes());
    }
    std::shared_ptr<const engine::AnimationSet> clips;
    engine::StagingBytes animation_memory;
    {
        const auto encoded = assets.read("facility.eani", 1024 * 1024);
        const engine::StagingBytes encoded_memory(encoded.capacity());
        clips = std::make_shared<const engine::AnimationSet>(
            engine::decode_animations(encoded, "facility.eani"));
        animation_memory.set(clips->buffer_bytes());
    }
    scene_memory.set(0);
    tile_memory.set(0);
    animation_memory.set(0);
    return Game(std::move(scene), std::move(tiles), std::move(clips), level);
}
// Saves/replays already carry content identity. Resolve installed levels without
// trusting a filename from an external record, preserving the existing formats.
inline void restore_game(Game& game, const Checkpoint& state,
                         const engine::AssetRoot* assets = nullptr,
                         const std::function<void(const Game&)>& prepare = {}) {
    if (state.content == game.content_id()) {
        game.restore(state, prepare);
        return;
    }
    if (assets) {
        auto candidate = load_game(*assets, next_level(game.level));
        if (candidate.content_id() == state.content) {
            candidate.restore(state);
            if (prepare)
                prepare(candidate);
            game.replace(std::move(candidate));
            return;
        }
    }
    throw std::runtime_error("Checkpoint content differs from installed levels");
}

struct Replay {
    static constexpr std::array<Vec2, 10> route = {{{-22, -10},
                                                    {-22, 8},
                                                    {-22, 0},
                                                    {-8, 0},
                                                    {0, -9},
                                                    {0, 0},
                                                    {16, 0},
                                                    {22, 8},
                                                    {28, 8},
                                                    {28, -10}}};
    static constexpr std::array<Vec2, 7> annex_route{
        {{-10, 0}, {-10, 6}, {-10, 0}, {10, 0}, {10, 6}, {10, -6}, {15, -6}}};
    std::size_t waypoint{};
    engine::InputFrame next(const Game& game) {
        const std::span<const Vec2> points = game.level == Level::facility
                                                 ? std::span<const Vec2>(route)
                                                 : std::span<const Vec2>(annex_route);
        while (waypoint < points.size() && std::hypot(game.position.x - points[waypoint].x,
                                                      game.position.y - points[waypoint].y) < 0.16F)
            ++waypoint;
        engine::InputFrame frame{};
        if (waypoint == points.size())
            return frame;
        const auto delta = points[waypoint] - game.position;
        frame[static_cast<std::size_t>(engine::Key::right)].held = delta.x > 0.08F;
        frame[static_cast<std::size_t>(engine::Key::left)].held = delta.x < -0.08F;
        frame[static_cast<std::size_t>(engine::Key::up)].held = delta.y > 0.08F;
        frame[static_cast<std::size_t>(engine::Key::down)].held = delta.y < -0.08F;
        return frame;
    }
};
} // namespace feature_lab
