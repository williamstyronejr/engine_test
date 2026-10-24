#include "allocation_profile.hpp"
#include <array>
#include <atomic>
#include <cstdlib>
#include <locale>
#include <new>
#include <sstream>

namespace allocation_profile {
namespace {
thread_local Count current;
struct Counters {
    std::atomic<std::uint64_t> samples{}, nonzero{}, calls{}, bytes{}, max_calls{}, max_bytes{};
};
std::array<Counters, scope_count> totals;
void maximum(std::atomic<std::uint64_t>& value, std::uint64_t candidate) noexcept {
    auto old = value.load(std::memory_order_relaxed);
    while (old < candidate &&
           !value.compare_exchange_weak(old, candidate, std::memory_order_relaxed)) {
    }
}
} // namespace
Count snapshot() noexcept {
    return current;
}
void record(Scope scope, Count before, Count after) noexcept {
    auto& t = totals[scope];
    const auto calls = after.calls - before.calls, bytes = after.bytes - before.bytes;
    t.samples.fetch_add(1, std::memory_order_relaxed);
    t.nonzero.fetch_add(calls != 0, std::memory_order_relaxed);
    t.calls.fetch_add(calls, std::memory_order_relaxed);
    t.bytes.fetch_add(bytes, std::memory_order_relaxed);
    maximum(t.max_calls, calls);
    maximum(t.max_bytes, bytes);
}
Summary summary(Scope scope) noexcept {
    const auto& t = totals[scope];
    return {t.samples.load(), t.nonzero.load(),   t.calls.load(),
            t.bytes.load(),   t.max_calls.load(), t.max_bytes.load()};
}
std::string json() {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\"schema\":\"cpp_new_scopes_v1\",\"unit\":\"requested_bytes\",\"scopes\":{";
    constexpr std::array names{
        "simulation", "minimap_submit", "world_submit",       "ui_submit",      "present",
        "frame",      "audio_producer", "audio_commands_mix", "audio_transport"};
    for (std::size_t i = 0; i < scope_count; ++i) {
        const auto s = summary(static_cast<Scope>(i));
        out << (i ? "," : "") << '"' << names[i] << "\":{\"samples\":" << s.samples
            << ",\"nonzero_samples\":" << s.nonzero_samples << ",\"calls\":" << s.calls
            << ",\"requested_bytes\":" << s.bytes << ",\"max_calls\":" << s.max_calls
            << ",\"max_requested_bytes\":" << s.max_bytes << '}';
    }
    return out.str() + "}}";
}
[[gnu::noinline]] void* allocate(std::size_t size, std::size_t alignment) {
    for (;;) {
        void* result{};
        if (alignment <= alignof(std::max_align_t))
            result = std::malloc(size ? size : 1);
        else if (posix_memalign(&result, alignment, size ? size : 1) != 0)
            result = nullptr;
        if (result) {
            ++current.calls;
            current.bytes += size;
            return result;
        }
        const auto handler = std::get_new_handler();
        if (!handler)
            throw std::bad_alloc();
        handler();
    }
}
} // namespace allocation_profile

void* operator new(std::size_t n) {
    return allocation_profile::allocate(n, alignof(std::max_align_t));
}
void* operator new[](std::size_t n) {
    return ::operator new(n);
}
void* operator new(std::size_t n, std::align_val_t a) {
    return allocation_profile::allocate(n, static_cast<std::size_t>(a));
}
void* operator new[](std::size_t n, std::align_val_t a) {
    return ::operator new(n, a);
}
void* operator new(std::size_t n, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(n);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t n, const std::nothrow_t& t) noexcept {
    return ::operator new(n, t);
}
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept {
    try {
        return ::operator new(n, a);
    } catch (...) {
        return nullptr;
    }
}
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t& t) noexcept {
    return ::operator new(n, a, t);
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete[](void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete(void* p, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete(void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}
void operator delete[](void* p, const std::nothrow_t&) noexcept {
    std::free(p);
}
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept {
    std::free(p);
}
