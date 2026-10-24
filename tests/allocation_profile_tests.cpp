#include "../profiling/allocation_profile.hpp"
#include "test.hpp"
#include <array>
#include <new>
#include <thread>

namespace {
using namespace testing;
using namespace allocation_profile;
TEST(replaceable_forms_count_requested_bytes_once) {
    // Direct calls prevent permitted new-expression allocation elision.
    const auto before = snapshot();
    void* a = ::operator new(0);
    void* b = ::operator new[](31);
    void* c = ::operator new(64, std::align_val_t{64});
    void* d = ::operator new[](128, std::align_val_t{128});
    void* e = ::operator new(7, std::nothrow);
    void* f = ::operator new[](9, std::nothrow);
    void* g = ::operator new(13, std::align_val_t{64}, std::nothrow);
    void* h = ::operator new[](15, std::align_val_t{64}, std::nothrow);
    const auto after = snapshot();
    CHECK(after.calls - before.calls == 8 && after.bytes - before.bytes == 267);
    CHECK(reinterpret_cast<std::uintptr_t>(c) % 64 == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(d) % 128 == 0);
    CHECK(reinterpret_cast<std::uintptr_t>(g) % 64 == 0);
    ::operator delete(a);
    ::operator delete[](b);
    ::operator delete(c, std::align_val_t{64});
    ::operator delete[](d, std::align_val_t{128});
    ::operator delete(e, std::nothrow);
    ::operator delete[](f, std::nothrow);
    ::operator delete(g, std::align_val_t{64}, std::nothrow);
    ::operator delete[](h, std::align_val_t{64}, std::nothrow);
    CHECK(snapshot().calls == after.calls);
}
TEST(scopes_are_inclusive_and_bookkeeping_does_not_allocate) {
    const auto before = snapshot();
    {
        Sample outer(frame);
        {
            Sample inner(ui);
            void* p = ::operator new(47);
            ::operator delete(p);
        }
    }
    record(ui, snapshot(), snapshot());
    const auto f = summary(frame), u = summary(ui);
    CHECK(f.samples == 1 && f.calls == 1 && f.bytes == 47);
    CHECK(u.samples == 2 && u.nonzero_samples == 1 && u.calls == 1 && u.max_bytes == 47);
    CHECK(snapshot().calls - before.calls == 1);
}
TEST(thread_snapshots_are_isolated_and_aggregates_join_cleanly) {
    std::array<std::thread, 2> threads;
    for (auto& thread : threads)
        thread = std::thread([] {
            const auto before = snapshot();
            for (int i = 0; i < 100; ++i) {
                Sample sample(audio_mix);
                void* p = ::operator new(19);
                ::operator delete(p);
            }
            if (snapshot().calls - before.calls != 100)
                std::terminate();
        });
    const auto before = snapshot();
    for (auto& thread : threads)
        thread.join();
    CHECK(snapshot().calls == before.calls);
    const auto s = summary(audio_mix);
    CHECK(s.samples == 200 && s.nonzero_samples == 200 && s.calls == 200);
    CHECK(s.bytes == 3800 && s.max_calls == 1 && s.max_bytes == 19);
    const auto report = json();
    CHECK(report.find("cpp_new_scopes_v1") != std::string::npos);
    CHECK(report.find("\"requested_bytes\":3800") != std::string::npos);
}
} // namespace
int main() {
    return testing::run_tests();
}
