#pragma once
#include "engine/entity.hpp"
#include <cstddef>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace engine {
// Dense compaction and scene teardown must not throw. Components can own resources
// and need not be default-constructible or copyable. Special members must not
// access the owning scene. Pools and their payloads are confined to one thread.
template <class T>
concept Component =
    std::is_object_v<T> && !std::is_array_v<T> && std::is_same_v<T, std::remove_cv_t<T>> &&
    std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T> &&
    std::is_nothrow_destructible_v<T>;

namespace detail {
class ComponentStorage {
  public:
    virtual ~ComponentStorage() = default;
    virtual void erase(Entity entity) noexcept = 0;
    virtual std::size_t buffer_bytes() const noexcept = 0;
};

template <Component T> class ComponentPool final : public ComponentStorage {
  public:
    struct Entry {
        Entity entity;
        T value;
        template <class... Args>
        Entry(Entity owner, Args&&... args) : entity(owner), value(std::forward<Args>(args)...) {}
    };
    explicit ComponentPool(std::size_t capacity) : sparse_(capacity, missing) {
        dense_.reserve(capacity);
    }
    T* find(Entity entity) noexcept {
        const auto index = locate(entity);
        return index == missing ? nullptr : &dense_[index].value;
    }
    const T* find(Entity entity) const noexcept {
        const auto index = locate(entity);
        return index == missing ? nullptr : &dense_[index].value;
    }
    template <class... Args> T& add(Entity entity, Args&&... args) {
        if (entity.index >= sparse_.size() || dense_.size() == sparse_.size())
            throw std::length_error("Component pool capacity exceeded");
        if (sparse_[entity.index] != missing)
            throw std::logic_error("Entity already has this component");
        // No allocation after pool creation. Publish the sparse entry only after
        // construction succeeds, so a throwing user constructor leaves no record.
        dense_.emplace_back(entity, std::forward<Args>(args)...);
        sparse_[entity.index] = dense_.size() - 1;
        return dense_.back().value;
    }
    void erase(Entity entity) noexcept override {
        const auto index = locate(entity);
        if (index == missing)
            return;
        if (index != dense_.size() - 1) {
            dense_[index] = std::move(dense_.back());
            sparse_[dense_[index].entity.index] = index;
        }
        dense_.pop_back();
        sparse_[entity.index] = missing;
    }
    std::span<Entry> entries() { return dense_; }
    std::span<const Entry> entries() const { return dense_; }
    std::size_t size() const { return dense_.size(); }
    std::size_t buffer_bytes() const noexcept override {
        return sparse_.capacity() * sizeof(std::size_t) + dense_.capacity() * sizeof(Entry);
    }

  private:
    static constexpr std::size_t missing = std::numeric_limits<std::size_t>::max();
    std::size_t locate(Entity entity) const noexcept {
        if (entity.index >= sparse_.size())
            return missing;
        const auto index = sparse_[entity.index];
        return index != missing && dense_[index].entity == entity ? index : missing;
    }
    std::vector<std::size_t> sparse_;
    std::vector<Entry> dense_;
};
} // namespace detail
} // namespace engine
