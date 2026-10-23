#pragma once
#include <string>
#include <string_view>

namespace feature_lab {
inline std::string json_string(std::string_view text) {
    std::string result = "\"";
    constexpr std::string_view hex = "0123456789abcdef";
    for (const char character : text) {
        const auto c = static_cast<unsigned char>(character);
        if (c == '"' || c == '\\') {
            result += '\\';
            result += static_cast<char>(c);
        } else if (c < 32 || c >= 127) {
            // Escape bytes rather than trusting external metadata to be valid UTF-8.
            result += "\\u00";
            result += hex[c >> 4];
            result += hex[c & 15];
        } else
            result += static_cast<char>(c);
    }
    return result + '"';
}
} // namespace feature_lab
