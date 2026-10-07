#pragma once
#include "engine/assets.hpp"
#include <optional>

namespace engine {
// CRC32 detects accidental corruption, not malicious modification.
std::uint32_t crc32(std::span<const std::uint8_t> bytes);
constexpr std::size_t max_record_bytes = 64 * 1024;
std::vector<std::uint8_t> seal_record(std::vector<std::uint8_t> payload);
std::span<const std::uint8_t> open_record(std::span<const std::uint8_t> bytes);
struct UserPaths {
    std::filesystem::path config, state;
    static UserPaths discover(const std::filesystem::path& override_root = {});
};
// Local, trusted per-user storage. Reads never create directories; writes are atomic.
class UserStorage {
  public:
    explicit UserStorage(UserPaths paths);
    const UserPaths& paths() const { return paths_; }
    std::optional<std::vector<std::uint8_t>> read_config() const;
    std::optional<std::vector<std::uint8_t>> read_slot(unsigned slot) const;
    void write_config(std::span<const std::uint8_t> bytes) const;
    void write_slot(unsigned slot, std::span<const std::uint8_t> bytes) const;

  private:
    UserPaths paths_;
};
} // namespace engine
