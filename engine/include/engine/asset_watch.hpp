#pragma once
#include "engine/assets.hpp"
#include <array>
#include <chrono>
#include <cstdint>

namespace engine {
struct WatchedAsset {
    std::string key;
    std::uint32_t groups{};
};
// Linux, thread-confined, nonblocking notifications with bounded metadata recovery.
// Notifications are hints, not permission to bypass normal asset validation.
class AssetWatcher {
  public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    static constexpr std::size_t max_files = 128, max_read_bytes = 16384;
    static constexpr auto quiet_period = std::chrono::milliseconds(150);
    static constexpr auto scan_period = std::chrono::seconds(1);
    AssetWatcher(const AssetRoot& root, std::span<const WatchedAsset> files);
    ~AssetWatcher();
    AssetWatcher(const AssetWatcher&) = delete;
    AssetWatcher& operator=(const AssetWatcher&) = delete;
    AssetWatcher(AssetWatcher&&) = delete;
    AssetWatcher& operator=(AssetWatcher&&) = delete;
    // Initial synchronization reports all groups after the quiet period. Thereafter
    // failed reloads are retried only on another observed change or explicit rescan.
    std::uint32_t poll(Time now = Clock::now());
    void request_rescan() { rescan_requested_ = true; }
    bool notifications_active() const { return fd_ >= 0 && complete_; }
    std::size_t size() const { return files_.size(); }
    std::uint64_t recoveries() const { return recoveries_; }

  private:
    struct Stamp {
        std::uint64_t device{}, inode{}, size{}, mode{};
        std::int64_t modified_seconds{}, modified_nanos{}, changed_seconds{}, changed_nanos{};
        bool operator==(const Stamp&) const = default;
    };
    struct File {
        std::filesystem::path path;
        std::string name;
        std::size_t directory{};
        std::uint32_t groups{};
        Stamp stamp;
    };
    struct Directory {
        std::filesystem::path path;
        Stamp stamp;
        int descriptor{-1};
    };
    Stamp inspect(const std::filesystem::path& path) const;
    void rebuild();
    void close() noexcept;
    std::uint32_t scan();
    void mark(std::uint32_t groups, Time now);
    std::filesystem::path root_;
    std::vector<File> files_;
    std::vector<Directory> directories_;
    std::array<Time, 32> deadlines_{};
    std::uint32_t all_groups_{}, pending_{};
    Time last_{}, next_scan_{};
    int fd_{-1};
    bool complete_{}, started_{}, rescan_requested_{true};
    std::uint64_t recoveries_{};
};
} // namespace engine
