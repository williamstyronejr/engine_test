#pragma once
#include "engine/animation.hpp"
#include "engine/assets.hpp"
#include "engine/collision.hpp"
#include "engine/collision_world.hpp"
#include "engine/input.hpp"
#include "engine/persistence.hpp"
#include "engine/scene.hpp"
#include "engine/tilemap.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace feature_lab {
using engine::Vec2;
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
};
struct Game {
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
    bool paused{}, won{}, door_started{}, door_open{};
    unsigned door_completions{};
    float camera_height{14};
    engine::CollisionWorld collisions;
    std::uint64_t alarm_entries{};
    bool alarm_disabled{}, alarm_touching{};
    static constexpr Vec2 switch_position{-27, -7.5F};
    Vec2 alarm_position() const {
        const auto phase = ticks % 240;
        const auto distance = phase <= 120 ? phase : 240 - phase;
        return {-24, -12 + static_cast<float>(distance) / 30};
    }
    Game(std::string scene_text, engine::TileMapData tiles,
         std::shared_ptr<const engine::AnimationSet> clips)
        : map(std::move(tiles)), animations(std::move(clips)), player_animation(animations),
          core_animation(animations), machine_animation(animations), door_animation(animations),
          initial_(std::move(scene_text)) {
        restart();
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
    Vec2 location(engine::Entity entity) const {
        const auto t = world.world_transform(entity);
        return {t.x, t.y};
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
        if ((paused && !single_step) || won)
            return 0;
        ++ticks;
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
        for (std::size_t i = 0; i < keys.size(); ++i) {
            if (collected[i])
                continue;
            const auto p = location(keys[i]);
            if (std::hypot(position.x - p.x, position.y - p.y) < 0.65F) {
                collected[i] = true;
                ++picked;
                world.defer_destroy(keys[i]);
            }
        }
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
        return picked;
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
        Game candidate(initial_, map.data(), animations);
        candidate.apply_checkpoint(state);
    }
    void restore(const Checkpoint& state) {
        if (state.content != content_id())
            throw std::runtime_error("Checkpoint content differs from installed facility");
        Game candidate(initial_, map.data(), animations);
        candidate.apply_checkpoint(state);
        // Commit world first: its move validates scene iteration before changing anything.
        // All following moves use standard allocators and cannot fail.
        world = std::move(candidate.world);
        position = previous = candidate.position;
        camera_height = candidate.camera_height;
        ticks = candidate.ticks;
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
    void update_sensors(bool prime) {
        using namespace engine;
        sensor_bodies_.clear();
        sensor_bodies_.push_back({1, Shape2D::box(position, {0.3F, 0.3F}), 1, 2, true, false});
        sensor_bodies_.push_back(
            {2, Shape2D::circle(alarm_position(), 0.7F), 2, alarm_disabled ? 0U : 1U, true, true});
        sensor_bodies_.push_back(
            {3, Shape2D::box(switch_position, {0.4F, 0.4F}), 4, 0, false, true});
        for (std::size_t i = 0; i < sensor_walls_.size(); ++i) {
            const auto r = sensor_walls_[i];
            sensor_bodies_.push_back({1000 + i,
                                      Shape2D::box((r.min + r.max) * 0.5F, (r.max - r.min) * 0.5F),
                                      8, 0, false, false});
        }
        if (!door_open)
            sensor_bodies_.push_back(
                {4, Shape2D::box(location(exit), {1.2F, 0.7F}), 8, 0, false, false});
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
        const auto d = switch_position - position;
        if (std::hypot(d.x, d.y) > 3)
            return;
        const auto hit = collisions.segment(position, switch_position, {4 | 8, 1, true});
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
        paused = state.paused;
        won = state.won;
        door_started = state.door_started;
        door_completions = door_open ? 1U : 0U;
        alarm_entries = state.alarm_entries;
        alarm_disabled = state.alarm_disabled;
        update_sensors(true);
    }
    void restart() {
        world = engine::parse_scene(initial_, "facility.scene");
        walls.clear();
        keys.clear();
        core_ids.clear();
        draw_order.clear();
        player = {};
        exit = {};
        world.each([&](const engine::SceneNode& node) {
            if (node.sprite) {
                draw_order.push_back(node.entity);
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
                core_ids.push_back(node.id);
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
            }
        });
        if (!player || !exit || keys.empty() || keys.size() > 64)
            throw std::runtime_error("Feature Lab requires player, exit and 1..64 cores");
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
        append_obstacles(map.bounds(), collision_candidates);
        for (const auto& wall : collision_candidates)
            if (engine::overlaps({position - Vec2{0.3F, 0.3F}, position + Vec2{0.3F, 0.3F}}, wall))
                throw std::runtime_error("Player starts inside a static collider");
        collected.assign(keys.size(), false);
        ticks = 0;
        paused = false;
        won = false;
        alarm_entries = 0;
        alarm_disabled = false;
        sensor_walls_.clear();
        map.append_colliders(map.bounds(), sensor_walls_);
        sensor_walls_.insert(sensor_walls_.end(), walls.begin(), walls.end());
        if (sensor_walls_.size() + 4 > engine::CollisionWorld::max_bodies)
            throw std::runtime_error("Feature Lab collision station exceeds body capacity");
        sensor_bodies_.reserve(engine::CollisionWorld::max_bodies);
        sensor_query_.reserve(engine::CollisionWorld::max_bodies);
        update_sensors(true);
    }
};
inline Game load_game(const engine::AssetRoot& assets) {
    return Game(
        assets.text("facility.scene"),
        engine::decode_tilemap(assets.read("facility.etmp", 4 * 1024 * 1024), "facility.etmp"),
        std::make_shared<const engine::AnimationSet>(
            engine::decode_animations(assets.read("facility.eani", 1024 * 1024), "facility.eani")));
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
    std::size_t waypoint{};
    engine::InputFrame next(const Game& game) {
        while (waypoint < route.size() && std::hypot(game.position.x - route[waypoint].x,
                                                     game.position.y - route[waypoint].y) < 0.16F)
            ++waypoint;
        engine::InputFrame frame{};
        if (waypoint == route.size())
            return frame;
        const auto delta = route[waypoint] - game.position;
        frame[static_cast<std::size_t>(engine::Key::right)].held = delta.x > 0.08F;
        frame[static_cast<std::size_t>(engine::Key::left)].held = delta.x < -0.08F;
        frame[static_cast<std::size_t>(engine::Key::up)].held = delta.y > 0.08F;
        frame[static_cast<std::size_t>(engine::Key::down)].held = delta.y < -0.08F;
        return frame;
    }
};
} // namespace feature_lab
