#include "engine/asset_watch.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

namespace engine {
AssetWatcher::AssetWatcher(const AssetRoot& root, std::span<const WatchedAsset> files)
    : root_(root.directory()) {
    if (files.empty() || files.size() > max_files)
        throw std::invalid_argument("Asset watcher requires 1..128 files");
    files_.reserve(files.size());
    directories_.reserve(files.size());
    for (const auto& file : files) {
        AssetRoot::validate_key(file.key);
        if (!file.groups)
            throw std::invalid_argument("Watched asset needs a nonzero group mask");
        const auto path = root_ / file.key;
        if (std::any_of(files_.begin(), files_.end(),
                        [&](const auto& f) { return f.path == path; }))
            throw std::invalid_argument("Duplicate watched asset key");
        const auto parent = path.parent_path();
        auto directory = std::find_if(directories_.begin(), directories_.end(),
                                      [&](const auto& d) { return d.path == parent; });
        if (directory == directories_.end()) {
            directories_.push_back({parent, {}, -1});
            directory = directories_.end() - 1;
        }
        files_.push_back({path, path.filename().string(),
                          static_cast<std::size_t>(directory - directories_.begin()), file.groups,
                          inspect(path)});
        all_groups_ |= file.groups;
    }
    // Keep owning the fd only once all throwing setup work has completed.
    rebuild();
}
AssetWatcher::~AssetWatcher() {
    close();
}
void AssetWatcher::close() noexcept {
    if (fd_ >= 0)
        ::close(fd_);
    fd_ = -1;
    complete_ = false;
    for (auto& directory : directories_)
        directory.descriptor = -1;
}
AssetWatcher::Stamp AssetWatcher::inspect(const std::filesystem::path& path) const {
    // Match AssetRoot's containment rule, including symlink changes. Missing and
    // inaccessible paths share a sentinel; creation/restored access changes it.
    std::error_code error;
    const auto canonical = std::filesystem::weakly_canonical(path, error);
    if (error)
        return {};
    const auto relative = canonical.lexically_relative(root_);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
        return {};
    struct stat value{};
    if (::stat(canonical.c_str(), &value) != 0)
        return {};
    return {static_cast<std::uint64_t>(value.st_dev),
            static_cast<std::uint64_t>(value.st_ino),
            static_cast<std::uint64_t>(value.st_size),
            static_cast<std::uint64_t>(value.st_mode),
            value.st_mtim.tv_sec,
            value.st_mtim.tv_nsec,
            value.st_ctim.tv_sec,
            value.st_ctim.tv_nsec};
}
void AssetWatcher::rebuild() {
    // Inspect first: allocation failure must not strand a newly opened descriptor.
    for (auto& directory : directories_)
        directory.stamp = inspect(directory.path);
    close();
    fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd_ < 0)
        return; // Metadata polling remains available; retry notifications at the next scan.
    complete_ = true;
    constexpr auto mask = IN_MODIFY | IN_CLOSE_WRITE | IN_ATTRIB | IN_CREATE | IN_DELETE |
                          IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR;
    for (auto& directory : directories_) {
        if (S_ISDIR(directory.stamp.mode))
            directory.descriptor = inotify_add_watch(fd_, directory.path.c_str(), mask);
        if (directory.descriptor < 0)
            complete_ = false;
    }
}
std::uint32_t AssetWatcher::scan() {
    std::uint32_t groups = 0;
    for (auto& file : files_) {
        const auto current = inspect(file.path);
        if (current != file.stamp) {
            file.stamp = current;
            groups |= file.groups;
        }
    }
    return groups;
}
void AssetWatcher::mark(std::uint32_t groups, Time now) {
    pending_ |= groups;
    for (unsigned bit = 0; bit < 32; ++bit)
        if (groups & (std::uint32_t{1} << bit))
            deadlines_[bit] = now + quiet_period;
}
std::uint32_t AssetWatcher::poll(Time now) {
    if (started_ && now < last_)
        throw std::invalid_argument("Asset watcher time must be monotonic");
    started_ = true;
    last_ = now;
    bool rebuild_needed = false, uncertain = false;
    std::uint32_t changed = 0;
    std::array<char, 4096> buffer{};
    if (fd_ >= 0) {
        // Four reads bound work even when a writer never stops. Reset the instance
        // on saturation/overflow, discarding stale watch descriptors and queued events.
        for (std::size_t attempt = 0; attempt < max_read_bytes / buffer.size(); ++attempt) {
            const auto count = ::read(fd_, buffer.data(), buffer.size());
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                break;
            if (count < 0 && errno == EINTR) {
                uncertain = true;
                break;
            }
            if (count <= 0) {
                uncertain = true;
                break;
            }
            const auto size = static_cast<std::size_t>(count);
            std::size_t offset = 0;
            while (offset < size) {
                inotify_event event{};
                if (size - offset < sizeof(event)) {
                    uncertain = true;
                    break;
                }
                std::memcpy(&event, buffer.data() + offset, sizeof(event));
                offset += sizeof(event);
                if (event.len > size - offset) {
                    uncertain = true;
                    break;
                }
                const std::string_view name(buffer.data() + offset,
                                            strnlen(buffer.data() + offset, event.len));
                offset += event.len;
                if (event.mask &
                    (IN_Q_OVERFLOW | IN_IGNORED | IN_UNMOUNT | IN_DELETE_SELF | IN_MOVE_SELF)) {
                    uncertain = true;
                    break;
                }
                for (const auto& file : files_)
                    if (directories_[file.directory].descriptor == event.wd && file.name == name)
                        changed |= file.groups;
            }
            if (uncertain)
                break;
            if (attempt + 1 == max_read_bytes / buffer.size())
                uncertain = true;
        }
    }
    if (now >= next_scan_) {
        next_scan_ = now + scan_period;
        rebuild_needed = !notifications_active();
        for (const auto& directory : directories_) {
            const auto current = inspect(directory.path);
            if (current.device != directory.stamp.device ||
                current.inode != directory.stamp.inode || current.mode != directory.stamp.mode)
                rebuild_needed = true;
        }
        changed |= scan();
    } else if (changed || uncertain || rescan_requested_) {
        changed |= scan(); // Update baseline now; the periodic scan must not repeat this reload.
    }
    if (uncertain || rescan_requested_) {
        changed |= all_groups_;
        rebuild_needed = true;
        rescan_requested_ = false;
        ++recoveries_;
    }
    if (rebuild_needed)
        rebuild();
    mark(changed, now);
    std::uint32_t ready = 0;
    for (unsigned bit = 0; bit < 32; ++bit) {
        const auto group = std::uint32_t{1} << bit;
        if ((pending_ & group) && now >= deadlines_[bit])
            ready |= group;
    }
    pending_ &= ~ready;
    return ready;
}
} // namespace engine
