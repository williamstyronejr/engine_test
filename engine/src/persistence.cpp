#include "engine/persistence.hpp"
#include <cstdlib>
#include <stdexcept>

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
std::vector<std::uint8_t> seal_record(std::vector<std::uint8_t> payload) {
    if (payload.size() < 8 || payload.size() > max_record_bytes - 4)
        throw std::invalid_argument("Persistence record size outside bounds");
    const auto crc = crc32(payload);
    for (unsigned i = 0; i < 4; ++i)
        payload.push_back(static_cast<std::uint8_t>(crc >> (i * 8)));
    return payload;
}
std::span<const std::uint8_t> open_record(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 12 || bytes.size() > max_record_bytes)
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
void write(const std::filesystem::path& path, std::string_view key,
           std::span<const std::uint8_t> bytes) {
    open_record(bytes); // Never replace a valid file with an incomplete envelope.
    std::filesystem::create_directories(path);
    AssetRoot(path).write_atomic(key, bytes);
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
