#pragma once
#include <array>
#include <charconv>
#include <concepts>
#include <limits>
#include <string>
#include <string_view>

namespace feature_lab {
namespace text_detail {
inline void append(std::string& output, std::string_view value) {
    output.append(value);
}
template <std::integral T>
    requires(!std::same_as<T, bool>)
void append(std::string& output, T value) {
    std::array<char, std::numeric_limits<T>::digits10 + 3> digits{};
    const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), value);
    output.append(digits.data(), static_cast<std::size_t>(result.ptr - digits.data()));
}
} // namespace text_detail
// Reuse caller-owned storage without intermediate strings. No truncation: longer
// text may grow the buffer. Inputs must not alias output; the returned view lasts
// until output is next changed/destroyed. Renderer::text consumes it immediately.
template <class... Parts> std::string_view format_text(std::string& output, const Parts&... parts) {
    output.clear();
    (text_detail::append(output, parts), ...);
    return output;
}
} // namespace feature_lab
