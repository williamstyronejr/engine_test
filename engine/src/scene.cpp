#include "engine/scene.hpp"
#include "engine/assets.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace engine {
std::size_t Scene::buffer_bytes() const {
    return slots_.capacity() * sizeof(Slot) + nodes_.capacity() * sizeof(SceneNode) +
           pending_.capacity() * sizeof(Entity);
}

namespace {
// Identity only, never gameplay state. Distinguishes handles from separate/reloaded worlds.
std::uint64_t new_domain() {
    static std::atomic<std::uint64_t> next{1};
    const auto value = next.fetch_add(1, std::memory_order_relaxed);
    if (!value)
        throw std::overflow_error("Scene domain exhausted");
    return value;
}
void label_valid(std::string_view text) {
    if (text.size() > 120)
        throw std::invalid_argument("Scene label exceeds 120 bytes");
    for (char c : text)
        if (c < 32 || c > 126)
            throw std::invalid_argument("Scene labels must be printable ASCII");
}
bool finite(Vec2 v) {
    return std::isfinite(v.x) && std::isfinite(v.y);
}
void transform_valid(LocalTransform t) {
    if (!finite(t.position) || !finite(t.scale) || !std::isfinite(t.rotation) ||
        std::abs(t.position.x) > 1e6F || std::abs(t.position.y) > 1e6F ||
        std::abs(t.scale.x) < 1e-4F || std::abs(t.scale.y) < 1e-4F || std::abs(t.scale.x) > 1e4F ||
        std::abs(t.scale.y) > 1e4F || std::abs(t.rotation) > 1e6F)
        throw std::invalid_argument("Invalid or out-of-range local transform");
}
} // namespace
Scene::Scene() : domain_(new_domain()) {
    slots_.reserve(capacity);
    nodes_.reserve(capacity);
    pending_.reserve(capacity);
}
Scene::Scene(Scene&& other) : Scene() {
    *this = std::move(other);
}
Scene& Scene::operator=(Scene&& other) {
    if (this == &other)
        return *this;
    require_mutable();
    other.require_mutable();
    const auto replacement_domain = new_domain();
    domain_ = other.domain_;
    slots_ = std::move(other.slots_);
    nodes_ = std::move(other.nodes_);
    ids_ = std::move(other.ids_);
    pending_ = std::move(other.pending_);
    other.slots_.clear();
    other.nodes_.clear();
    other.ids_.clear();
    other.pending_.clear();
    other.domain_ = replacement_domain;
    return *this;
}
void Scene::require_mutable() const {
    if (iteration_depth_)
        throw std::logic_error("Scene mutation during iteration; defer destruction until flush");
}
Entity Scene::create(std::uint64_t id, std::string name, std::string tag) {
    require_mutable();
    label_valid(name);
    label_valid(tag);
    if (!id || ids_.contains(id))
        throw std::invalid_argument("Persistent entity IDs must be unique and nonzero");
    if (nodes_.size() >= capacity)
        throw std::runtime_error("Scene entity limit reached");
    std::size_t index = 0;
    while (index < slots_.size() &&
           (slots_[index].alive ||
            slots_[index].generation == std::numeric_limits<std::uint64_t>::max()))
        ++index;
    const bool appended = index == slots_.size();
    if (appended && slots_.size() == capacity)
        throw std::runtime_error("Scene handle slots exhausted");
    if (appended)
        slots_.push_back({});
    const Entity entity{static_cast<std::uint32_t>(index), slots_[index].generation, domain_};
    try {
        ids_.emplace(id, entity);
        try {
            nodes_.push_back({entity, id, std::move(name), std::move(tag), {}, {}, {}, {}});
        } catch (...) {
            ids_.erase(id);
            throw;
        }
    } catch (...) {
        if (appended)
            slots_.pop_back();
        throw;
    }
    slots_[index].alive = true;
    slots_[index].dense = static_cast<std::uint32_t>(nodes_.size() - 1);
    return entity;
}
bool Scene::valid(Entity e) const {
    return e.domain == domain_ && e.index < slots_.size() && slots_[e.index].alive &&
           slots_[e.index].generation == e.generation;
}
Entity Scene::find(std::uint64_t id) const {
    const auto found = ids_.find(id);
    return found == ids_.end() ? Entity{} : found->second;
}
const SceneNode& Scene::get(Entity e) const {
    if (!valid(e))
        throw std::invalid_argument("Invalid, foreign, or stale entity handle");
    return nodes_[slots_[e.index].dense];
}
SceneNode& Scene::mutable_node(Entity e) {
    get(e);
    return nodes_[slots_[e.index].dense];
}
void Scene::set_transform(Entity e, LocalTransform t) {
    require_mutable();
    transform_valid(t);
    mutable_node(e).local = t;
}
void Scene::set_parent(Entity child, Entity parent) {
    require_mutable();
    get(child);
    if (parent)
        get(parent);
    std::size_t ancestors = 0;
    for (auto p = parent; p; p = get(p).parent) {
        if (p == child)
            throw std::invalid_argument("Hierarchy cycle");
        if (++ancestors >= max_depth)
            throw std::invalid_argument("Hierarchy exceeds depth limit");
    }
    for (const auto& node : nodes_) {
        std::size_t descendants = 0;
        for (auto p = node.entity; p; p = get(p).parent) {
            if (p == child) {
                if (ancestors + descendants + 1 > max_depth)
                    throw std::invalid_argument("Reparent exceeds subtree depth limit");
                break;
            }
            ++descendants;
        }
    }
    mutable_node(child).parent = parent;
}
void Scene::set_sprite(Entity e, Sprite sprite) {
    require_mutable();
    AssetRoot::validate_key(sprite.texture);
    if (!finite(sprite.size) || sprite.size.x <= 0 || sprite.size.y <= 0 || sprite.size.x > 1e6F ||
        sprite.size.y > 1e6F || std::abs(static_cast<long long>(sprite.layer)) > 100000)
        throw std::invalid_argument("Invalid sprite dimensions/layer");
    for (float c : {sprite.r, sprite.g, sprite.b, sprite.a})
        if (!std::isfinite(c) || c < 0 || c > 1)
            throw std::invalid_argument("Sprite color must be finite in [0,1]");
    mutable_node(e).sprite = std::move(sprite);
}
void Scene::set_collider(Entity e, Collider c) {
    require_mutable();
    if (!finite(c.half) || c.half.x <= 0 || c.half.y <= 0 || c.half.x > 1e6F || c.half.y > 1e6F)
        throw std::invalid_argument("Invalid collider extents");
    mutable_node(e).collider = c;
}
Transform Scene::world_transform(Entity e) const {
    Transform result;
    for (auto p = e; p; p = get(p).parent) {
        const auto& t = get(p).local;
        result = Transform::from(t.position, t.rotation, t.scale) * result;
    }
    if (!valid(e))
        throw std::invalid_argument("Invalid entity transform request");
    for (float value : {result.a, result.b, result.c, result.d, result.x, result.y})
        if (!std::isfinite(value) || std::abs(value) > 1e12F)
            throw std::runtime_error("World transform exceeds numeric limits");
    return result;
}
void Scene::destroy(Entity e) {
    require_mutable();
    get(e);
    std::array<bool, capacity> removed{};
    // Discover the entire subtree before mutating storage.
    for (const auto& node : nodes_)
        for (auto p = node.entity; p; p = get(p).parent)
            if (p == e) {
                removed[node.entity.index] = true;
                break;
            }
    std::size_t i = 0;
    while (i < nodes_.size()) {
        if (!removed[nodes_[i].entity.index]) {
            ++i;
            continue;
        }
        auto& slot = slots_[nodes_[i].entity.index];
        ids_.erase(nodes_[i].id);
        slot.alive = false;
        ++slot.generation;
        if (i != nodes_.size() - 1) {
            nodes_[i] = std::move(nodes_.back());
            slots_[nodes_[i].entity.index].dense = static_cast<std::uint32_t>(i);
        }
        nodes_.pop_back();
    }
}
void Scene::defer_destroy(Entity e) {
    get(e);
    if (std::find(pending_.begin(), pending_.end(), e) == pending_.end())
        pending_.push_back(e);
}
void Scene::flush() {
    require_mutable();
    for (auto e : pending_)
        if (valid(e))
            destroy(e);
    pending_.clear();
}
namespace {
std::vector<std::string> tokens(std::string_view line) {
    if (line.size() > 4096)
        throw std::runtime_error("Line exceeds 4096 bytes");
    std::vector<std::string> result;
    std::size_t i = 0;
    while (i < line.size()) {
        if (line[i] == ' ' || line[i] == '\t' || line[i] == '\r') {
            ++i;
            continue;
        }
        if (line[i] == '#')
            break;
        std::string token;
        if (line[i] == '"') {
            ++i;
            bool closed = false;
            while (i < line.size()) {
                char c = line[i++];
                if (c == '"') {
                    closed = true;
                    break;
                }
                if (c == '\\') {
                    if (i == line.size() || (line[i] != '\\' && line[i] != '"'))
                        throw std::runtime_error("Invalid quoted escape");
                    c = line[i++];
                }
                if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) > 126)
                    throw std::runtime_error("Non-ASCII token");
                token += c;
            }
            if (!closed)
                throw std::runtime_error("Unterminated quoted token");
            if (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' &&
                line[i] != '#')
                throw std::runtime_error("Missing token separator");
        } else {
            const auto start = i;
            while (i < line.size() && line[i] != ' ' && line[i] != '\t' && line[i] != '\r' &&
                   line[i] != '#')
                ++i;
            token = line.substr(start, i - start);
            for (char c : token)
                if (c < 32 || c > 126 || c == '"' || c == '\\')
                    throw std::runtime_error("Invalid bare token");
        }
        result.push_back(std::move(token));
        if (result.size() > 16)
            throw std::runtime_error("Too many tokens");
    }
    return result;
}
template <class T> T number(const std::string& text) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size())
        throw std::runtime_error("Invalid number: " + text);
    if constexpr (std::is_floating_point_v<T>)
        if (!std::isfinite(value))
            throw std::runtime_error("Nonfinite number");
    return value;
}
void count(const std::vector<std::string>& t, std::size_t expected) {
    if (t.size() != expected)
        throw std::runtime_error("Wrong field count for " + t.front());
}
} // namespace
Scene parse_scene(std::string_view text, std::string_view source) {
    if (text.size() > 4 * 1024 * 1024)
        throw std::runtime_error(std::string(source) + ": scene exceeds 4 MiB");
    Scene result;
    struct Parent {
        Entity child;
        std::uint64_t parent;
        std::size_t line;
    };
    std::vector<Parent> parents;
    std::size_t line_number = 0, offset = 0;
    bool header = false, ended = false;
    try {
        while (offset < text.size()) {
            ++line_number;
            const auto newline = text.find('\n', offset);
            const auto end = newline == std::string_view::npos ? text.size() : newline;
            const auto t = tokens(text.substr(offset, end - offset));
            offset = end + 1;
            if (t.empty())
                continue;
            if (ended)
                throw std::runtime_error("Data after end");
            if (!header) {
                count(t, 2);
                if (t[0] != "scene" || t[1] != "1")
                    throw std::runtime_error("Expected scene version 1");
                header = true;
                continue;
            }
            if (t[0] == "end") {
                count(t, 1);
                ended = true;
                continue;
            }
            if (t[0] == "entity") {
                count(t, 10);
                const auto e = result.create(number<std::uint64_t>(t[1]), t[2], t[3]);
                result.set_transform(e, {{number<float>(t[5]), number<float>(t[6])},
                                         number<float>(t[7]),
                                         {number<float>(t[8]), number<float>(t[9])}});
                parents.push_back({e, number<std::uint64_t>(t[4]), line_number});
            } else if (t[0] == "sprite") {
                count(t, 10);
                const auto e = result.find(number<std::uint64_t>(t[1]));
                if (result.get(e).sprite)
                    throw std::runtime_error("Duplicate sprite component");
                result.set_sprite(e, {t[2],
                                      {number<float>(t[3]), number<float>(t[4])},
                                      number<float>(t[5]),
                                      number<float>(t[6]),
                                      number<float>(t[7]),
                                      number<float>(t[8]),
                                      number<int>(t[9])});
            } else if (t[0] == "collider") {
                count(t, 4);
                const auto e = result.find(number<std::uint64_t>(t[1]));
                if (result.get(e).collider)
                    throw std::runtime_error("Duplicate collider component");
                result.set_collider(e, {{number<float>(t[2]), number<float>(t[3])}});
            } else
                throw std::runtime_error("Unknown record: " + t[0]);
        }
        if (!header || !ended)
            throw std::runtime_error("Incomplete scene (header/end required)");
        for (const auto& p : parents)
            if (p.parent) {
                line_number = p.line;
                const auto parent = result.find(p.parent);
                if (!parent)
                    throw std::runtime_error("Unknown parent persistent ID");
                result.set_parent(p.child, parent);
            }
        for (const auto& p : parents) {
            line_number = p.line;
            result.world_transform(p.child);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string(source) + ":" + std::to_string(line_number) + ": " +
                                 e.what());
    }
    return result;
}
std::string serialize_scene(const Scene& scene) {
    std::vector<const SceneNode*> nodes;
    scene.each([&](const SceneNode& node) { nodes.push_back(&node); });
    std::sort(nodes.begin(), nodes.end(),
              [](const auto* a, const auto* b) { return a->id < b->id; });
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<float>::max_digits10);
    output << "scene 1\n";
    for (const auto* n : nodes) {
        scene.world_transform(n->entity);
        output << "entity " << n->id << ' ' << std::quoted(n->name) << ' ' << std::quoted(n->tag)
               << ' ' << (n->parent ? scene.get(n->parent).id : 0) << ' ' << n->local.position.x
               << ' ' << n->local.position.y << ' ' << n->local.rotation << ' ' << n->local.scale.x
               << ' ' << n->local.scale.y << '\n';
        if (n->sprite) {
            const auto& s = *n->sprite;
            output << "sprite " << n->id << ' ' << std::quoted(s.texture) << ' ' << s.size.x << ' '
                   << s.size.y << ' ' << s.r << ' ' << s.g << ' ' << s.b << ' ' << s.a << ' '
                   << s.layer << '\n';
        }
        if (n->collider)
            output << "collider " << n->id << ' ' << n->collider->half.x << ' '
                   << n->collider->half.y << '\n';
    }
    output << "end\n";
    return output.str();
}
void replace_scene(Scene& live, std::string_view text, std::string_view source) {
    Scene candidate = parse_scene(text, source);
    live = std::move(candidate);
}
} // namespace engine
