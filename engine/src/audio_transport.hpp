#pragma once
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace engine::audio_detail {
struct TransportStats {
    std::uint64_t written_frames{}, underruns{}, suspensions{}, recoveries{}, waits{};
    int error{}; // Latched negative device error; zero while healthy.
};
// Worker-owned transport state. No allocation, mixing or unbounded retry loop.
// Device supplies nonblocking write/prepare/resume, a bounded wait, and backoff.
class BlockWriter {
  public:
    static constexpr std::size_t frames = 256;
    using Buffer = std::array<float, frames * 2>;
    bool needs_block() const { return offset_ == frames && !stats_.error; }
    void begin_block() { offset_ = 0; }
    TransportStats stats() const { return stats_; }
    template <class Device> void step(Device& device, const Buffer& buffer) {
        if (stats_.error || needs_block())
            return;
        if (suspended_) {
            const int result = device.resume();
            if (result == -EAGAIN || result == -EINTR) {
                device.backoff(); // Return to the shutdown check after at most one short sleep.
                return;
            }
            if (result < 0) {
                prepare(device);
                return;
            }
            suspended_ = false;
            ++stats_.recoveries;
            return;
        }
        const auto result = device.write(buffer.data() + offset_ * 2, frames - offset_);
        if (result > 0) {
            if (static_cast<std::uint64_t>(result) > frames - offset_) {
                stats_.error = -EIO; // Defensive: a backend must not exceed the requested span.
                return;
            }
            offset_ += static_cast<std::size_t>(result);
            stats_.written_frames += static_cast<std::uint64_t>(result);
        } else if (result == 0 || result == -EAGAIN) {
            ++stats_.waits;
            const int ready = device.wait();
            if (ready < 0)
                handle_error(device, ready);
        } else {
            handle_error(device, static_cast<int>(result));
        }
    }

  private:
    template <class Device> void prepare(Device& device) {
        const int result = device.prepare();
        if (result < 0)
            stats_.error = result;
        else {
            suspended_ = false;
            ++stats_.recoveries;
        }
    }
    template <class Device> void handle_error(Device& device, int error) {
        if (error == -EINTR || error == -EAGAIN)
            return;
        if (error == -EPIPE) {
            ++stats_.underruns;
            prepare(device);
        } else if (error == -ESTRPIPE) {
            ++stats_.suspensions;
            suspended_ = true;
        } else {
            stats_.error = error;
        }
    }
    std::size_t offset_{frames};
    bool suspended_{};
    TransportStats stats_;
};
} // namespace engine::audio_detail
