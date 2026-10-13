#pragma once
#include <cstdint>
#include <limits>

namespace engine {
struct Entity {
    std::uint32_t index{std::numeric_limits<std::uint32_t>::max()};
    std::uint64_t generation{}, domain{};
    bool operator==(const Entity&) const = default;
    explicit operator bool() const { return domain != 0; }
};
} // namespace engine
