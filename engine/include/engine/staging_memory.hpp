#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace engine {
// Explicitly instrumented payloads, not allocator/RSS/driver memory. CPU counts
// buffer capacities or source bytes; GPU counts requested uncompressed texels.
struct StagingSnapshot {
    bool measured{};
    std::size_t cpu_peak{}, gpu_peak{}, combined_peak{}, cpu_remaining{}, gpu_remaining{};
    bool clean() const { return !cpu_remaining && !gpu_remaining; }
};
enum class StagingKind { cpu, gpu };
class StagingBytes;
// One synchronous operation on this thread. All leases must end before this
// scope. Disabled instrumentation allocates nothing and retains no global data.
class StagingScope {
  public:
    StagingScope() {
        if (active_)
            throw std::logic_error("Nested staging measurement");
        active_ = this;
        state_.measured = true;
    }
    ~StagingScope() { active_ = nullptr; }
    StagingScope(const StagingScope&) = delete;
    StagingScope& operator=(const StagingScope&) = delete;
    StagingSnapshot snapshot() const { return state_; }

  private:
    friend class StagingBytes;
    inline static thread_local StagingScope* active_{};
    StagingSnapshot state_{};
    void add(StagingKind kind, std::size_t bytes) {
        const auto total = state_.cpu_remaining + state_.gpu_remaining;
        if (bytes > std::numeric_limits<std::size_t>::max() - total)
            throw std::overflow_error("Staging byte counter overflow");
        (kind == StagingKind::cpu ? state_.cpu_remaining : state_.gpu_remaining) += bytes;
        state_.cpu_peak = std::max(state_.cpu_peak, state_.cpu_remaining);
        state_.gpu_peak = std::max(state_.gpu_peak, state_.gpu_remaining);
        state_.combined_peak = std::max(state_.combined_peak, total + bytes);
    }
    void remove(StagingKind kind, std::size_t bytes) noexcept {
        (kind == StagingKind::cpu ? state_.cpu_remaining : state_.gpu_remaining) -= bytes;
    }
};
// A lease follows staging ownership until disposal or publication as live data.
// Shared snapshots get one lease per payload, not one per shared_ptr.
class StagingBytes {
  public:
    explicit StagingBytes(std::size_t bytes = 0, StagingKind kind = StagingKind::cpu)
        : scope_(StagingScope::active_), kind_(kind) {
        set(bytes);
    }
    ~StagingBytes() {
        if (scope_)
            scope_->remove(kind_, bytes_);
    }
    StagingBytes(const StagingBytes&) = delete;
    StagingBytes& operator=(const StagingBytes&) = delete;
    StagingBytes(StagingBytes&& other) noexcept
        : scope_(std::exchange(other.scope_, nullptr)), kind_(other.kind_), bytes_(other.bytes_) {}
    StagingBytes& operator=(StagingBytes&&) = delete;
    void set(std::size_t bytes) {
        if (!scope_)
            return;
        if (bytes > bytes_)
            scope_->add(kind_, bytes - bytes_);
        else
            scope_->remove(kind_, bytes_ - bytes);
        bytes_ = bytes;
    }

  private:
    StagingScope* scope_{};
    StagingKind kind_{};
    std::size_t bytes_{};
};
} // namespace engine
