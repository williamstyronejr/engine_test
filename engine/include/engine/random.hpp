#pragma once
#include <cstdint>

namespace engine {
// SplitMix64: deterministic simulation state, not cryptographic randomness.
// Algorithm reference: https://prng.di.unimi.it/splitmix64.c (public domain).
class Random {
  public:
    static constexpr std::uint64_t increment = 0x9e3779b97f4a7c15ULL;
    explicit constexpr Random(std::uint64_t seed = 1) : state_(seed) {}
    constexpr std::uint64_t state() const { return state_; }
    constexpr void restore(std::uint64_t state) { state_ = state; }
    constexpr void advance(std::uint64_t count) { state_ += increment * count; }
    constexpr std::uint64_t value() const {
        auto value = state_;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }
    constexpr std::uint64_t next() {
        advance(1);
        return value();
    }

  private:
    std::uint64_t state_;
};
} // namespace engine
