#pragma once
#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace engine::binary {
class Reader {
  public:
    Reader(std::span<const std::uint8_t> bytes, std::string_view magic, std::uint32_t version = 1)
        : bytes_(bytes) {
        if (magic.size() != 4 || bytes.size() < 8 || std::memcmp(bytes.data(), magic.data(), 4))
            fail("Invalid " + std::string(magic) + " header");
        offset_ = 4;
        if (u32() != version)
            fail("Unsupported format version");
    }
    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error(message + " at byte " + std::to_string(offset_));
    }
    void require(std::size_t count) const {
        if (count > bytes_.size() - offset_)
            fail("Truncated file");
    }
    std::uint32_t u32() {
        require(4);
        std::uint32_t value = 0;
        for (unsigned int i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(bytes_[offset_++]) << (i * 8);
        return value;
    }
    std::uint64_t u64() {
        const auto low = u32();
        return low | (static_cast<std::uint64_t>(u32()) << 32);
    }
    bool boolean() {
        const auto value = u32();
        if (value > 1)
            fail("Invalid boolean");
        return value != 0;
    }
    std::uint16_t u16() {
        require(2);
        const auto low = bytes_[offset_++];
        return static_cast<std::uint16_t>(static_cast<unsigned int>(low) |
                                          (static_cast<unsigned int>(bytes_[offset_++]) << 8));
    }
    std::int32_t i32() { return std::bit_cast<std::int32_t>(u32()); }
    float f32() { return std::bit_cast<float>(u32()); }
    std::span<const std::uint8_t> bytes(std::size_t count) {
        require(count);
        const auto result = bytes_.subspan(offset_, count);
        offset_ += count;
        return result;
    }
    std::string string(std::size_t limit) {
        const auto size = u32();
        if (size > limit)
            fail("String limit exceeded");
        require(size);
        std::string value(reinterpret_cast<const char*>(bytes_.data() + offset_), size);
        offset_ += size;
        return value;
    }
    void finish() const {
        if (offset_ != bytes_.size())
            fail("Trailing data");
    }

  private:
    std::span<const std::uint8_t> bytes_;
    std::size_t offset_{};
};
class Writer {
  public:
    explicit Writer(std::string_view magic, std::uint32_t version = 1) {
        for (char c : magic)
            bytes_.push_back(static_cast<std::uint8_t>(c));
        u32(version);
    }
    void u32(std::uint32_t value) {
        for (unsigned int i = 0; i < 4; ++i)
            bytes_.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
    }
    void u64(std::uint64_t value) {
        u32(static_cast<std::uint32_t>(value));
        u32(static_cast<std::uint32_t>(value >> 32));
    }
    void u16(std::uint16_t value) {
        bytes_.push_back(static_cast<std::uint8_t>(value));
        bytes_.push_back(static_cast<std::uint8_t>(value >> 8));
    }
    void i32(std::int32_t value) { u32(std::bit_cast<std::uint32_t>(value)); }
    void f32(float value) { u32(std::bit_cast<std::uint32_t>(value)); }
    void string(std::string_view value) {
        u32(static_cast<std::uint32_t>(value.size()));
        for (char c : value)
            bytes_.push_back(static_cast<std::uint8_t>(c));
    }
    void bytes(std::span<const std::uint8_t> value) {
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }
    std::vector<std::uint8_t> take() { return std::move(bytes_); }

  private:
    std::vector<std::uint8_t> bytes_;
};
} // namespace engine::binary
