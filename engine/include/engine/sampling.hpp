#pragma once
#include <initializer_list>
#include <stdexcept>

namespace engine {
enum class TextureFilter { nearest, linear };
enum class TextureWrap { clamp, repeat };
struct TextureSampling {
    TextureFilter min{TextureFilter::nearest}, mag{TextureFilter::nearest};
    TextureWrap u{TextureWrap::clamp}, v{TextureWrap::clamp};
    bool operator==(const TextureSampling&) const = default;
    void validate() const {
        for (auto filter : {min, mag})
            if (filter != TextureFilter::nearest && filter != TextureFilter::linear)
                throw std::invalid_argument("Invalid texture filter");
        for (auto wrap : {u, v})
            if (wrap != TextureWrap::clamp && wrap != TextureWrap::repeat)
                throw std::invalid_argument("Invalid texture wrap");
    }
};
} // namespace engine
