#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// Linked only into dedicated profiling executables. Snapshots belong to the
// calling thread; aggregates are read after measurement/worker shutdown.
namespace allocation_profile {
struct Count {
    std::uint64_t calls{}, bytes{};
};
enum Scope : std::size_t {
    simulation,
    minimap,
    world,
    ui,
    present,
    frame,
    audio_producer,
    audio_mix,
    audio_transport,
    scope_count
};
struct Summary {
    std::uint64_t samples{}, nonzero_samples{}, calls{}, bytes{}, max_calls{}, max_bytes{};
};
Count snapshot() noexcept;
void record(Scope scope, Count before, Count after) noexcept;
Summary summary(Scope scope) noexcept;
std::string json();
class Sample {
  public:
    explicit Sample(Scope scope) noexcept : scope_(scope), before_(snapshot()) {}
    ~Sample() { record(scope_, before_, snapshot()); }
    Sample(const Sample&) = delete;
    Sample& operator=(const Sample&) = delete;

  private:
    Scope scope_;
    Count before_;
};
} // namespace allocation_profile
