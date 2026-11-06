#pragma once

#include "decimals.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

inline constexpr std::int64_t MAX_PRICE_TICKS = 10'000'000'000LL;  // |price| <= $1e6
inline constexpr std::int64_t MAX_QTY_UNITS   = 100'000'000LL;     // |qty|   <= 1e6 contracts

inline bool in_magnitude(std::int64_t v, std::int64_t maxMag) {
    return v >= -maxMag && v <= maxMag;
}

inline std::optional<std::int64_t> parse_price(std::string_view s) {
    auto v = parse_decimal<4>(s);
    if (!v || !in_magnitude(*v, MAX_PRICE_TICKS)) return std::nullopt;
    return v;
}

inline std::optional<std::int64_t> parse_quantity(std::string_view s) {
    auto v = parse_decimal<2>(s);
    if (!v || !in_magnitude(*v, MAX_PRICE_TICKS)) return std::nullopt;
    return v;
}