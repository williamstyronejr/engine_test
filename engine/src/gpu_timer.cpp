#include "engine/gpu_timer.hpp"
#include "gpu_timer_backend.hpp"
#include "gpu_timer_queue.hpp"

namespace engine {
struct GpuTimer::Impl {
    detail::TimerBackend backend;
    detail::GpuTimerQueue<detail::TimerBackend> queue{backend, backend.bits};
};
GpuTimer::GpuTimer() : impl_(std::make_unique<Impl>()) {
}
GpuTimer::~GpuTimer() = default;
bool GpuTimer::begin(std::uint64_t frame) {
    return impl_->queue.begin(frame);
}
void GpuTimer::end() {
    impl_->queue.end();
}
std::span<const GpuTimingSample> GpuTimer::collect() {
    return impl_->queue.collect();
}
GpuTimingStats GpuTimer::stats() const {
    return impl_->queue.stats();
}
} // namespace engine
