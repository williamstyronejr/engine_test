#include "engine/assets.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

namespace engine {
namespace {
constexpr std::size_t max_texture_bytes = 16 * 1024 * 1024;
std::uint32_t word(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}
std::size_t image_bytes(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || width > 4096 || height > 4096)
        throw std::runtime_error("Texture dimensions must be in 1..4096");
    const auto count = static_cast<std::uint64_t>(width) * height * 4;
    if (count > max_texture_bytes)
        throw std::runtime_error("Texture exceeds 16 MiB decoded limit");
    return static_cast<std::size_t>(count);
}
void system_failure(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}
} // namespace
void TextureData::validate() const {
    if (rgba.size() != image_bytes(width, height))
        throw std::runtime_error("Texture payload size mismatch");
}
TextureData decode_texture(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 16 || std::memcmp(bytes.data(), "ETEX", 4) != 0)
        throw std::runtime_error("Expected ETEX texture header");
    if (word(bytes, 4) != 1)
        throw std::runtime_error("Unsupported ETEX version");
    const auto width = word(bytes, 8), height = word(bytes, 12);
    if (bytes.size() - 16 != image_bytes(width, height))
        throw std::runtime_error("Truncated or oversized ETEX payload");
    return {width, height, {bytes.begin() + 16, bytes.end()}};
}
std::vector<std::uint8_t> encode_texture(const TextureData& texture) {
    texture.validate();
    std::vector<std::uint8_t> result;
    result.reserve(16 + texture.rgba.size());
    for (auto byte : std::array<std::uint8_t, 4>{'E', 'T', 'E', 'X'})
        result.push_back(byte);
    for (const auto value : {1U, texture.width, texture.height})
        for (unsigned int shift = 0; shift < 32; shift += 8)
            result.push_back(static_cast<std::uint8_t>(value >> shift));
    result.insert(result.end(), texture.rgba.begin(), texture.rgba.end());
    return result;
}
AssetRoot::AssetRoot(std::filesystem::path directory)
    : directory_(std::filesystem::canonical(directory)) {
    if (!std::filesystem::is_directory(directory_))
        throw std::runtime_error("Asset root is not a directory");
}
AssetRoot AssetRoot::discover(const std::filesystem::path& override_directory) {
    if (!override_directory.empty())
        return AssetRoot(override_directory);
    const auto executable = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    for (const auto& candidate : {executable / "assets", executable / "../share/feature_lab"})
        if (std::filesystem::is_directory(candidate))
            return AssetRoot(candidate);
    throw std::runtime_error("Cannot find assets beside the executable; use --assets DIRECTORY");
}
void AssetRoot::validate_key(std::string_view key) {
    if (key.empty() || key.size() > 240 || key.front() == '/' || key.back() == '/')
        throw std::runtime_error("Invalid relative asset key");
    std::size_t start = 0;
    for (std::size_t i = 0; i <= key.size(); ++i) {
        if (i == key.size() || key[i] == '/') {
            const auto part = key.substr(start, i - start);
            if (part.empty() || part == "." || part == "..")
                throw std::runtime_error("Invalid asset path component");
            start = i + 1;
        } else {
            const unsigned char c = static_cast<unsigned char>(key[i]);
            if (c < 32 || c >= 127 || c == '\\' || c == ':')
                throw std::runtime_error("Invalid asset path character");
        }
    }
}
std::filesystem::path AssetRoot::resolve(std::string_view key) const {
    validate_key(key);
    const auto path = std::filesystem::weakly_canonical(directory_ / std::string(key));
    const auto relative = path.lexically_relative(directory_);
    if (relative.empty() || *relative.begin() == ".." || relative.is_absolute())
        throw std::runtime_error("Asset path escapes its root");
    return path;
}
std::vector<std::uint8_t> AssetRoot::read(std::string_view key, std::size_t limit) const {
    try {
        const auto path = resolve(key);
        if (!std::filesystem::is_regular_file(path))
            throw std::runtime_error("Not a regular asset file");
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            throw std::runtime_error("Cannot open asset");
        const auto length = file.tellg();
        if (length < 0 || static_cast<std::uint64_t>(length) > limit)
            throw std::runtime_error("Asset exceeds size limit");
        std::vector<std::uint8_t> result(static_cast<std::size_t>(length));
        file.seekg(0);
        if (!result.empty())
            file.read(reinterpret_cast<char*>(result.data()),
                      static_cast<std::streamsize>(result.size()));
        if (!file || file.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("Asset changed or read failed");
        return result;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string(key) + ": " + e.what());
    }
}
std::string AssetRoot::text(std::string_view key, std::size_t limit) const {
    const auto bytes = read(key, limit);
    return {bytes.begin(), bytes.end()};
}
void AssetRoot::write_atomic(std::string_view key, std::span<const std::uint8_t> bytes,
                             AtomicWriteObserver observer) const {
    const auto target = resolve(key);
    const auto parent = target.parent_path();
    // The asset root is trusted local project storage, not an adversarial shared filesystem.
    std::string temporary = (parent / ".engine-save-XXXXXX").string();
    int fd = mkstemp(temporary.data());
    if (fd < 0)
        system_failure("Create save temporary");
    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto written = ::write(fd, bytes.data() + offset, bytes.size() - offset);
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0)
                system_failure("Write save temporary");
            offset += static_cast<std::size_t>(written);
        }
        if (fsync(fd) != 0)
            system_failure("Sync save temporary");
        const int closed = close(fd);
        fd = -1;
        if (closed != 0)
            system_failure("Close save temporary");
        if (observer)
            observer(AtomicWriteStage::file_synced);
        if (rename(temporary.c_str(), target.c_str()) != 0)
            system_failure("Replace saved file");
        if (observer)
            observer(AtomicWriteStage::renamed);
        const int directory = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (directory < 0)
            system_failure("Open save directory");
        const int synced = fsync(directory), saved_errno = errno;
        close(directory);
        if (synced != 0) {
            errno = saved_errno;
            system_failure("Sync save directory");
        }
    } catch (...) {
        if (fd >= 0)
            close(fd);
        unlink(temporary.c_str());
        throw;
    }
}
void AssetRoot::write_text(std::string_view key, std::string_view value) const {
    write_atomic(key, {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()});
}
void TextureCache::collect() {
    std::erase_if(textures_, [](const auto& entry) { return entry.second.expired(); });
}
std::shared_ptr<const TextureData> TextureCache::load(std::string_view key) {
    const auto found = textures_.find(std::string(key));
    if (found != textures_.end())
        if (auto snapshot = found->second.lock())
            return snapshot;
    return reload(key);
}
std::shared_ptr<const TextureData> TextureCache::reload(std::string_view key) {
    const auto bytes = root_.read(key, max_texture_bytes + 16);
    std::shared_ptr<const TextureData> texture;
    try {
        texture = std::make_shared<const TextureData>(decode_texture(bytes));
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string(key) + ": " + e.what());
    }
    collect();
    if (!textures_.contains(std::string(key)) && textures_.size() >= 64)
        throw std::runtime_error("Texture cache entry limit reached");
    textures_.insert_or_assign(std::string(key), texture);
    return texture;
}
std::shared_ptr<const TextureData> TextureCache::load_or_fallback(std::string_view key,
                                                                  std::string& error) {
    try {
        auto texture = load(key);
        error.clear();
        return texture;
    } catch (const std::exception& e) {
        error = e.what();
    }
    static const auto fallback = std::make_shared<const TextureData>(
        TextureData{2, 2, {255, 0, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 0, 255, 255}});
    return fallback;
}
} // namespace engine
