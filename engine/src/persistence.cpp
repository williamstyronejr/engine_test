#include "engine/persistence.hpp"
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>

namespace engine {
std::uint32_t crc32(std::span<const std::uint8_t> bytes) {
    std::uint32_t crc = 0xffffffffU;
    for (auto byte : bytes) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xedb88320U : 0U);
    }
    return ~crc;
}
std::vector<std::uint8_t> seal_record(std::vector<std::uint8_t> payload, std::size_t limit) {
    if (limit < 12 || payload.size() < 8 || payload.size() > limit - 4)
        throw std::invalid_argument("Persistence record size outside bounds");
    const auto crc = crc32(payload);
    for (unsigned i = 0; i < 4; ++i)
        payload.push_back(static_cast<std::uint8_t>(crc >> (i * 8)));
    return payload;
}
std::span<const std::uint8_t> open_record(std::span<const std::uint8_t> bytes, std::size_t limit) {
    if (bytes.size() < 12 || bytes.size() > limit)
        throw std::runtime_error("Invalid persistence record size");
    const auto payload = bytes.first(bytes.size() - 4);
    std::uint32_t expected{};
    for (unsigned i = 0; i < 4; ++i)
        expected |= static_cast<std::uint32_t>(bytes[payload.size() + i]) << (i * 8);
    if (crc32(payload) != expected)
        throw std::runtime_error("Persistence checksum mismatch");
    return payload;
}
namespace {
std::filesystem::path absolute_env(const char* name) {
    const char* value = std::getenv(name);
    if (value && std::filesystem::path(value).is_absolute())
        return value;
    return {};
}
std::string slot_key(unsigned slot) {
    if (slot < 1 || slot > 3)
        throw std::invalid_argument("Save slot must be 1..3");
    return "slot-" + std::to_string(slot) + ".esav";
}
std::optional<std::vector<std::uint8_t>> read(const std::filesystem::path& path,
                                              std::string_view key) {
    if (!std::filesystem::exists(path))
        return {};
    const AssetRoot root(path);
    if (!std::filesystem::exists(path / key))
        return {};
    return root.read(key, max_record_bytes);
}
[[noreturn]] void directory_failure(const char* operation, const std::filesystem::path& path,
                                   int error) {
    throw std::system_error(error, std::generic_category(),
                            std::string(operation) + ": " + path.string());
}
std::filesystem::path prepare_directories(const std::filesystem::path& path) {
    // Resolve existing symlinks before deciding which ancestor entries to synchronize.
    const auto resolved = std::filesystem::weakly_canonical(path);
    std::vector<std::filesystem::path> chain;
    for (auto current = resolved;; current = current.parent_path()) {
        chain.push_back(current);
        if (current == current.root_path())
            break;
    }
    // EEXIST may also mean a nondirectory; the subsequent opens verify the chain.
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        if (*it == it->root_path())
            continue;
        if (::mkdir(it->c_str(), 0777) != 0 && errno != EEXIST)
            directory_failure("Create save directory", *it, errno);
    }
    // Include existing ancestors on every attempt. A prior process may have created
    // them and failed before syncing; existence is not a durability acknowledgement.
    for (const auto& directory : chain) {
        const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0)
            directory_failure("Open save directory chain", directory, errno);
        const int synced = ::fsync(fd), sync_error = errno;
        const int closed = ::close(fd), close_error = errno;
        // Close exactly once, including after EINTR, and preserve the sync error.
        if (synced != 0)
            directory_failure("Sync save directory chain", directory, sync_error);
        if (closed != 0)
            directory_failure("Close save directory chain", directory, close_error);
    }
    return resolved;
}
void write(const std::filesystem::path& path, std::string_view key,
           std::span<const std::uint8_t> bytes) {
    open_record(bytes); // Never replace a valid file with an incomplete envelope.
    AssetRoot(prepare_directories(path)).write_atomic(key, bytes);
}
} // namespace
UserPaths UserPaths::discover(const std::filesystem::path& override_root) {
    if (!override_root.empty()) {
        const auto root = std::filesystem::absolute(override_root).lexically_normal();
        return {root / "config", root / "state"};
    }
    const auto home = absolute_env("HOME");
    auto config = absolute_env("XDG_CONFIG_HOME"), state = absolute_env("XDG_STATE_HOME");
    if ((config.empty() || state.empty()) && home.empty())
        throw std::runtime_error("Set absolute HOME/XDG directories or use --user-data DIRECTORY");
    if (config.empty())
        config = home / ".config";
    if (state.empty())
        state = home / ".local/state";
    return {config / "feature_lab", state / "feature_lab"};
}
UserStorage::UserStorage(UserPaths paths) : paths_(std::move(paths)) {
    if (!paths_.config.is_absolute() || !paths_.state.is_absolute())
        throw std::invalid_argument("User storage paths must be absolute");
}
std::optional<std::vector<std::uint8_t>> UserStorage::read_config() const {
    return read(paths_.config, "settings.ecfg");
}
std::optional<std::vector<std::uint8_t>> UserStorage::read_slot(unsigned slot) const {
    return read(paths_.state, slot_key(slot));
}
void UserStorage::write_config(std::span<const std::uint8_t> bytes) const {
    write(paths_.config, "settings.ecfg", bytes);
}
void UserStorage::write_slot(unsigned slot, std::span<const std::uint8_t> bytes) const {
    write(paths_.state, slot_key(slot), bytes);
}
} // namespace engine
