#include "engine/keyboard_symbols.hpp"
#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace engine {
namespace {
struct CharacterAlias {
    std::uint32_t symbol, point;
};
struct CharacterRange {
    std::uint32_t first, last;
};
#include "keyboard_data.inc"
std::uint32_t character_symbol(std::uint32_t point) {
    if (point >= 'A' && point <= 'Z')
        return point + ('a' - 'A');
    return point <= 0xff ? point : 0x01000000U + point;
}
} // namespace
std::uint32_t canonical_character_symbol(std::uint32_t symbol) {
    if (symbol <= 0xff)
        return character_symbol(symbol);
    if (symbol >= 0x01000000 && symbol <= 0x0110ffff)
        return character_symbol(symbol - 0x01000000);
    if (symbol < 0x100 || symbol > 0x20ff)
        return symbol;
    const auto* found =
        std::lower_bound(std::begin(character_aliases), std::end(character_aliases), symbol,
                         [](const auto& entry, auto value) { return entry.symbol < value; });
    return found != std::end(character_aliases) && found->symbol == symbol
               ? character_symbol(found->point)
               : symbol;
}
bool is_international_binding_symbol(std::uint32_t symbol) {
    std::uint32_t point{};
    if (symbol >= 0xa1 && symbol <= 0xff)
        point = symbol;
    else if (symbol >= 0x01000100 && symbol <= 0x0110ffff)
        point = symbol - 0x01000000;
    else
        return false;
    const auto* found =
        std::lower_bound(std::begin(character_ranges), std::end(character_ranges), point,
                         [](const auto& range, auto value) { return range.last < value; });
    return found != std::end(character_ranges) && found->first <= point;
}
std::string international_symbol_name(std::uint32_t symbol) {
    if (!is_international_binding_symbol(symbol))
        throw std::invalid_argument("Invalid international binding symbol");
    switch (symbol) {
    case 0xe0:
        return "AGRAVE";
    case 0xe4:
        return "AUML";
    case 0xe5:
        return "ARING";
    case 0xe7:
        return "CCEDIL";
    case 0xe8:
        return "EGRAVE";
    case 0xe9:
        return "EACUTE";
    case 0xea:
        return "ECIRC";
    case 0xf1:
        return "NTILDE";
    case 0xf6:
        return "OUML";
    case 0xfc:
        return "UUML";
    case 0xdf:
        return "SHARPS";
    default:
        break;
    }
    auto point = symbol >= 0x01000100 ? symbol - 0x01000000 : symbol;
    constexpr char digits[] = "0123456789ABCDEF";
    char buffer[6]{};
    std::size_t length = 0;
    do {
        buffer[length++] = digits[point & 15U];
        point >>= 4;
    } while (point || length < 4);
    std::string name = "U+";
    while (length)
        name += buffer[--length];
    return name;
}
} // namespace engine
