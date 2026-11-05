#pragma once

#include "types.hpp"
#include <cstdint>
#include <string_view>
#include <cmath>

/*
    Different quantities required different scaling
    Quantity => N == 2 == 10e2 scaling
    Price => N == 4 == 10e4 scaling
    Funding => N == 6 == 10e6 scaling
*/

constexpr std::uint64_t INTEGRAL_BASE {10};

template <int N>
std::int64_t scale_decimal(std::string_view stringWithoutDecimalPoint);

template <int N>
std::optional<std::int64_t> parse_decimal(std::string_view tickAsString) {

    auto decimalIdx = tickAsString.find('.');

    if (decimalIdx == std::string_view::npos) {
        return (scale_decimal<N>(tickAsString));
    }

    bool decimalPointFound{};
    int l{};
    int r{};

    std::string tickWithoutDecimalPoint {};
    tickWithoutDecimalPoint.reserve(tickAsString.size());

    for (int i = 0; char c : tickAsString) {
        if (c != '.') {
            decimalPointFound  = true;
            ++i;
            continue;
        }

        if (decimalPointFound) {
            ++r;           
        } else {
            ++l;
        }

        tickWithoutDecimalPoint[i] = c; 
        ++i;
    }

    return (scale_decimal<N>(tickWithoutDecimalPoint));


}

template <int N>
std::string format_decimal(std::int64_t decimal) {}

template <int N>
std::int64_t scale_decimal(std::string_view stringWithoutDecimalPoint) {

    std::int64_t res{};

    // more performant that stoi, places result in res if parseable
    // will not parse given a leading whitespace
    auto [ptr, ec] = std::from_chars(stringWithoutDecimalPoint.begin(), stringWithoutDecimalPoint.end(), res, INTEGRAL_BASE);

    // need to determine if we should throw here, this should never get hit
    // if (ec != std::errc::invalid_argument) {}

    return res * std::pow(INTEGRAL_BASE, N);

}

// parse_decimal<2>(1.20);