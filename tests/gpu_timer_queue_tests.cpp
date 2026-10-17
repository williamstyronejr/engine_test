#include "../engine/src/gpu_timer_queue.hpp"
#include "engine/diagnostics.hpp"
#include "test.hpp"
#include <limits>

namespace {
using namespace engine;
using namespace testing;
struct FakeBackend {
    struct Query {
        bool issued{}, ready{}, consumed{};
        std::uint64_t value{};
    };
    std::array<Query, GpuTimer::capacity * 2> queries{};
    unsigned stamps{}, probes{}, reads{};
    bool conflict{};
    void begin(std::size_t slot) {
        if (conflict)
            throw std::logic_error("active query");
        stamp(slot * 2);
    }
    void end(std::size_t slot) { stamp(slot * 2 + 1); }
    void stamp(std::size_t index) {
        auto& q = queries.at(index);
        CHECK(!q.issued || q.consumed); // Never overwrite an unread query.
        q = {true, false, false, 0};
        ++stamps;
    }
    bool available(std::size_t index) {
        ++probes;
        CHECK(queries.at(index).issued && !queries.at(index).consumed);
        return queries.at(index).ready;
    }
    std::uint64_t result(std::size_t index) {
        auto& q = queries.at(index);
        CHECK(q.issued && q.ready && !q.consumed); // An early read would block real GL.
        q.consumed = true;
        ++reads;
        return q.value;
    }
    void finish(std::size_t slot, std::uint64_t start, std::uint64_t end) {
        queries.at(slot * 2).value = start;
        queries.at(slot * 2 + 1).value = end;
        queries.at(slot * 2).ready = queries.at(slot * 2 + 1).ready = true;
    }
};
TEST(delayed_queries_never_read_unavailable_results) {
    FakeBackend backend;
    detail::GpuTimerQueue timer(backend, 64, GpuTimingMethod::timestamp_pair);
    CHECK(timer.begin(1));
    timer.end();
    CHECK(timer.collect().empty() && backend.probes == 1 && backend.reads == 0);
    backend.queries[1].ready = true;
    CHECK(timer.collect().empty() && backend.probes == 3 && backend.reads == 0);
    backend.finish(0, 1000000000000000ULL, 1000000001250000ULL);
    const auto samples = timer.collect();
    CHECK(samples.size() == 1 && samples[0].frame == 1);
    NEAR(samples[0].milliseconds, 1.25);
    CHECK(timer.stats().completed == 1 && timer.stats().pending == 0 && backend.reads == 2);
    CHECK(timer.collect().empty() && backend.reads == 2);
}
TEST(full_pool_skips_without_overwrite_and_recovers_in_fifo_order) {
    FakeBackend backend;
    detail::GpuTimerQueue timer(backend, 64, GpuTimingMethod::timestamp_pair);
    for (std::uint64_t frame = 1; frame <= GpuTimer::capacity; ++frame) {
        CHECK(timer.begin(frame));
        timer.end();
    }
    CHECK(!timer.begin(9));
    timer.end();
    CHECK(backend.stamps == 16 && timer.stats().pending == 8 && timer.stats().skipped == 1);
    backend.finish(1, 0, 2000000);
    CHECK(timer.collect().empty() && backend.reads == 0);
    backend.finish(0, 0, 1000000);
    const auto first = timer.collect();
    CHECK(first.size() == 2 && first[0].frame == 1 && first[1].frame == 2);
    CHECK(timer.begin(10)); // Reuse slot 0 only after its old result was consumed.
    timer.end();
    backend.finish(0, 0, 10000000);
    CHECK(timer.collect().empty());
    for (std::size_t i = 2; i < GpuTimer::capacity; ++i)
        backend.finish(i, 0, (i + 1) * 1000000);
    const auto rest = timer.collect();
    CHECK(rest.size() == 7 && rest.back().frame == 10);
    NEAR(rest.back().milliseconds, 10);
    for (std::size_t i = 0; i < 6; ++i)
        CHECK(rest[i].frame == i + 3);
    CHECK(timer.stats().submitted == 9 && timer.stats().completed == 9);
    CHECK(timer.stats().pending == 0 && backend.reads == 18);
}
TEST(scope_misuse_preserves_pending_results_and_tags) {
    FakeBackend backend;
    detail::GpuTimerQueue timer(backend, 64, GpuTimingMethod::timestamp_pair);
    rejects([&] { timer.end(); });
    rejects([&] { timer.begin(0); });
    CHECK(timer.begin(5));
    rejects([&] { timer.begin(6); });
    rejects([&] { timer.collect(); });
    timer.end();
    rejects([&] { timer.end(); });
    rejects([&] { timer.begin(5); });
    rejects([&] { timer.begin(4); });
    CHECK(timer.begin(6));
    timer.end();
    CHECK(timer.stats().pending == 2 && backend.stamps == 4);
    backend.finish(0, 1, 2);
    backend.finish(1, 3, 4);
    CHECK(timer.collect().size() == 2);
}
TEST(unsupported_counters_issue_no_queries) {
    for (const int bits : {0, 30, 32, 63, 65}) {
        FakeBackend backend;
        detail::GpuTimerQueue timer(backend, bits, GpuTimingMethod::timestamp_pair);
        CHECK(!timer.stats().supported && timer.stats().counter_bits == bits);
        CHECK(!timer.begin(1));
        rejects([&] { timer.collect(); });
        timer.end();
        CHECK(timer.collect().empty());
        CHECK(backend.stamps == 0 && backend.probes == 0 && backend.reads == 0);
        CHECK(timer.stats().submitted == 0 && timer.stats().skipped == 1);
    }
}
TEST(clock_reversal_zero_and_large_timestamps) {
    FakeBackend backend;
    detail::GpuTimerQueue timer(backend, 64, GpuTimingMethod::timestamp_pair);
    for (std::uint64_t i = 1; i <= 3; ++i) {
        CHECK(timer.begin(i));
        timer.end();
    }
    backend.finish(0, 100, 99);
    backend.finish(1, 100, 100);
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    backend.finish(2, maximum - 1000000, maximum);
    const auto samples = timer.collect();
    CHECK(samples.size() == 2 && samples[0].frame == 2 && samples[1].frame == 3);
    NEAR(samples[0].milliseconds, 0);
    NEAR(samples[1].milliseconds, 1);
    CHECK(timer.stats().invalid == 1 && timer.stats().completed == 2);
    CHECK(timer.stats().pending == 0);
    const auto failures = timer.failures();
    CHECK(failures.size() == 1 && failures[0].frame == 1);
    CHECK(failures[0].start_ns == 100 && failures[0].end_ns == 99);
    CHECK(failures[0].slot == 0 && failures[0].last_attempted_frame == 3);
}
TEST(failure_evidence_survives_slot_reuse_and_bounded_overflow) {
    FakeBackend backend;
    detail::GpuTimerQueue timer(backend, 64, GpuTimingMethod::timestamp_pair);
    constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    for (std::uint64_t frame = 1; frame <= 40; ++frame) {
        CHECK(timer.begin(frame));
        timer.end();
        const auto slot = static_cast<std::size_t>((frame - 1) % GpuTimer::capacity);
        // Alternate exact large reversed values and valid samples through five wraps.
        backend.finish(slot, maximum - frame,
                       frame % 2 ? maximum - frame - 1 : maximum - frame + 1);
        const auto reads = backend.reads, probes = backend.probes;
        const auto samples = timer.collect();
        CHECK(backend.reads == reads + 2 && backend.probes == probes + 2);
        CHECK(samples.size() == (frame % 2 ? 0 : 1));
        if (!samples.empty())
            CHECK(samples[0].frame == frame && samples[0].milliseconds == 0.000001);
        CHECK(timer.stats().pending == 0);
        CHECK(timer.stats().submitted == timer.stats().completed + timer.stats().invalid);
    }
    CHECK(timer.stats().completed == 20 && timer.stats().invalid == 20);
    CHECK(timer.failures().size() == GpuTimer::failure_capacity);
    for (std::size_t i = 0; i < timer.failures().size(); ++i) {
        const auto& failure = timer.failures()[i];
        const auto frame = i * 2 + 1;
        CHECK(failure.frame == frame && failure.last_attempted_frame == frame);
        CHECK(failure.start_ns == maximum - frame && failure.end_ns == maximum - frame - 1);
        CHECK(failure.slot == (frame - 1) % GpuTimer::capacity);
    }
    const auto reads = backend.reads;
    CHECK(timer.collect().empty() && backend.reads == reads);
    CHECK(timer.failures().front().frame == 1 && timer.failures().back().frame == 15);
    FakeBackend fresh_backend;
    detail::GpuTimerQueue fresh(fresh_backend, 64, GpuTimingMethod::timestamp_pair);
    CHECK(fresh.failures().empty() && fresh.stats().invalid == 0);
}
TEST(delayed_reversed_pair_keeps_original_tag_after_skipped_begin) {
    FakeBackend backend;
    detail::GpuTimerQueue timer(backend, 64, GpuTimingMethod::timestamp_pair);
    for (std::uint64_t frame = 1; frame <= GpuTimer::capacity; ++frame) {
        CHECK(timer.begin(frame * 100));
        timer.end();
    }
    CHECK(!timer.begin(900));
    timer.end();
    backend.queries[1].ready = true;
    CHECK(timer.collect().empty() && timer.failures().empty() && backend.reads == 0);
    backend.finish(0, 9007199254740993ULL, 9007199254740992ULL);
    CHECK(timer.collect().empty());
    CHECK(timer.failures().size() == 1);
    const auto first = timer.failures()[0];
    CHECK(first.frame == 100 && first.last_attempted_frame == 900 && first.slot == 0);
    CHECK(first.start_ns - first.end_ns == 1);
    CHECK(timer.begin(1000));
    timer.end();
    backend.finish(0, 50, 60);
    CHECK(timer.collect().empty()); // A ready reused slot cannot pass the unavailable head.
    for (std::size_t i = 1; i < GpuTimer::capacity; ++i)
        backend.finish(i, 10, 20);
    const auto completed = timer.collect();
    CHECK(completed.size() == 8 && completed.back().frame == 1000);
    CHECK(timer.failures()[0].start_ns == first.start_ns && timer.failures()[0].frame == 100);
    CHECK(timer.stats().invalid == 1 && timer.stats().completed == 8);
    CHECK(timer.stats().pending == 0 && timer.stats().skipped == 1);
}
TEST(repeated_pool_rollover_keeps_tags_and_bounds) {
    FakeBackend backend;
    detail::GpuTimerQueue timer(backend, 64, GpuTimingMethod::timestamp_pair);
    for (std::uint64_t batch = 0; batch < 100; ++batch) {
        for (std::uint64_t i = 0; i < GpuTimer::capacity; ++i) {
            CHECK(timer.begin(batch * GpuTimer::capacity + i + 1));
            timer.end();
            backend.finish(static_cast<std::size_t>(i), 20, 1000020);
        }
        const auto before = backend.probes;
        const auto samples = timer.collect();
        CHECK(samples.size() == GpuTimer::capacity);
        CHECK(backend.probes - before == 2 * GpuTimer::capacity);
        for (std::size_t i = 0; i < samples.size(); ++i) {
            CHECK(samples[i].frame == batch * GpuTimer::capacity + i + 1);
            NEAR(samples[i].milliseconds, 1);
        }
    }
    CHECK(timer.stats().completed == 800 && timer.stats().skipped == 0);
}
// Elapsed results are already durations; no absolute clock subtraction is involved.
struct ElapsedBackend : FakeBackend {
    void begin(std::size_t slot) {
        if (conflict)
            throw std::logic_error("active query");
        stamp(slot);
    }
    void end(std::size_t) {}
    void finish_elapsed(std::size_t slot, std::uint64_t value) {
        queries.at(slot).value = value;
        queries.at(slot).ready = true;
    }
};
TEST(elapsed_queries_poll_once_and_preserve_tags_across_pool_reuse) {
    ElapsedBackend backend;
    detail::GpuTimerQueue timer(backend, 64, GpuTimingMethod::elapsed);
    CHECK(timer.stats().method == GpuTimingMethod::elapsed);
    for (std::uint64_t batch = 0; batch < 100; ++batch) {
        for (std::uint64_t i = 0; i < GpuTimer::capacity; ++i) {
            CHECK(timer.begin(batch * 9 + i + 1));
            timer.end();
        }
        CHECK(!timer.begin(batch * 9 + 9));
        timer.end();
        const auto reads = backend.reads, probes = backend.probes;
        CHECK(timer.collect().empty());
        CHECK(backend.reads == reads && backend.probes == probes + 1);
        for (std::size_t i = 0; i < GpuTimer::capacity; ++i)
            backend.finish_elapsed(i, i * 1000000);
        const auto samples = timer.collect();
        CHECK(samples.size() == 8 && backend.reads == reads + 8);
        CHECK(backend.probes == probes + 9);
        for (std::size_t i = 0; i < samples.size(); ++i) {
            CHECK(samples[i].frame == batch * 9 + i + 1);
            NEAR(samples[i].milliseconds, static_cast<double>(i));
        }
    }
    CHECK(timer.stats().completed == 800 && timer.stats().skipped == 100);
    CHECK(timer.stats().invalid == 0 && timer.failures().empty());
}
TEST(elapsed_conflict_is_transactional_and_unsigned_results_are_not_subtracted) {
    ElapsedBackend backend;
    detail::GpuTimerQueue timer(backend, 64, GpuTimingMethod::elapsed);
    backend.conflict = true;
    rejects([&] { timer.begin(1); });
    rejects([&] { timer.end(); });
    CHECK(timer.collect().empty() && timer.stats().submitted == 0);
    backend.conflict = false;
    CHECK(timer.begin(1)); // A failed begin did not consume the frame tag.
    timer.end();
    backend.finish_elapsed(0, UINT64_MAX);
    const auto samples = timer.collect();
    CHECK(samples.size() == 1 && samples[0].milliseconds > 1e12);
    // Driver durations are trusted; absolute wrap repair and arbitrary caps are not applied.
    CHECK(timer.stats().invalid == 0 && timer.failures().empty());
    for (const int bits : {0, 30, 32, 63, 65}) {
        ElapsedBackend unsupported;
        detail::GpuTimerQueue narrow(unsupported, bits, GpuTimingMethod::elapsed);
        CHECK(!narrow.begin(1));
        narrow.end();
        CHECK(narrow.collect().empty() && unsupported.stamps == 0);
    }
}
TEST(gpu_history_uses_completed_samples_and_rejects_invalid_records) {
    GpuTimingHistory history;
    CHECK(history.summary().samples == 0);
    for (std::uint64_t i = 1; i <= 100; ++i)
        history.record({i * 3, static_cast<double>(i)}); // Gaps from skipped frames are valid.
    auto stats = history.summary();
    CHECK(stats.samples == 100 && stats.last_frame == 300);
    NEAR(stats.mean_ms, 50.5);
    NEAR(stats.p95_ms, 95);
    NEAR(stats.max_ms, 100);
    NEAR(stats.last_ms, 100);
    rejects([&] { history.record({300, 1}); });
    rejects([&] { history.record({0, 1}); });
    for (const double bad :
         {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        rejects([&] { history.record({301, bad}); });
    CHECK(history.summary().last_frame == 300 && history.summary().samples == 100);
    for (std::uint64_t i = 301; i <= 540; ++i)
        history.record({i, 2});
    stats = history.summary();
    CHECK(stats.samples == GpuTimingHistory::capacity && stats.last_frame == 540);
    NEAR(stats.mean_ms, 2);
    NEAR(stats.p95_ms, 2);
    history.clear();
    CHECK(history.summary().samples == 0 && history.summary().last_frame == 0);
    history.record({1, 0});
    NEAR(history.summary().last_ms, 0);
}
} // namespace
int main() {
    return testing::run_tests();
}
