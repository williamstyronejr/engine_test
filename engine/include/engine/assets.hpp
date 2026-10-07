#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace engine {
struct TextureData {
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> rgba;
    void validate() const;
};
TextureData decode_texture(std::span<const std::uint8_t> bytes);
std::vector<std::uint8_t> encode_texture(const TextureData& texture);

enum class AtomicWriteStage { file_synced, renamed };
using AtomicWriteObserver = void (*)(AtomicWriteStage);

class AssetRoot {
  public:
    explicit AssetRoot(std::filesystem::path directory);
    static AssetRoot discover(const std::filesystem::path& override_directory = {});
    const std::filesystem::path& directory() const { return directory_; }
    std::vector<std::uint8_t> read(std::string_view relative, std::size_t limit) const;
    std::string text(std::string_view relative, std::size_t limit = 4 * 1024 * 1024) const;
    // Write to a sibling temporary file, fsync, rename, then fsync the parent directory.
    void write_atomic(std::string_view relative, std::span<const std::uint8_t> bytes,
                      AtomicWriteObserver observer = nullptr) const;
    void write_text(std::string_view relative, std::string_view text) const;
    static void validate_key(std::string_view key);

  private:
    std::filesystem::path resolve(std::string_view relative) const;
    std::filesystem::path directory_;
};

// Immutable snapshots. Failed reloads preserve the cache; successful reloads leave
// existing readers valid until they explicitly acquire the replacement snapshot.
class TextureCache {
  public:
    explicit TextureCache(AssetRoot root) : root_(std::move(root)) {}
    std::shared_ptr<const TextureData> load(std::string_view key);
    std::shared_ptr<const TextureData> reload(std::string_view key);
    std::shared_ptr<const TextureData> load_or_fallback(std::string_view key, std::string& error);
    void collect();
    std::size_t entries() const { return textures_.size(); }

  private:
    AssetRoot root_;
    std::unordered_map<std::string, std::weak_ptr<const TextureData>> textures_;
};
} // namespace engine
