#pragma once

#include "decimals.hpp"
#include "types.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <limits>

inline constexpr std::int64_t MAX_PRICE_TICKS = 100'000'000LL;
inline constexpr std::int64_t MAX_QTY_UNITS   = 10'000'000'000LL;

static_assert(MAX_QTY_UNITS > 0);

static_assert(MAX_PRICE_TICKS <= std::numeric_limits<std::int64_t>::max() / 9 / MAX_QTY_UNITS,
              "MAX_PRICE_TICKS * MAX_QTY_UNITS leaves less than 9x headroom in int64");

inline bool in_magnitude(std::int64_t v, std::int64_t maxMag) {
    return v >= -maxMag && v <= maxMag;
}

inline std::optional<std::int64_t> parse_price(std::string_view s) {
    auto v = parse_decimal<PRICE_N>(s);
    if (!v || !in_magnitude(*v, MAX_PRICE_TICKS)) return std::nullopt;
    return v;
}

inline std::optional<std::int64_t> parse_quantity(std::string_view s) {
    auto v = parse_decimal<QUANTITY_N>(s);
    if (!v || !in_magnitude(*v, MAX_QTY_UNITS)) return std::nullopt;
    return v;
}