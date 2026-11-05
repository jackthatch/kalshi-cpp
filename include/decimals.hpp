#pragma once

#include "types.hpp"
#include <cstdint>

/*
    Different quantities required different scaling
    Quantity => N == 2 == 10e2 scaling
    Price => N == 4 == 10e4 scaling
    Funding => N == 6 == 10e6 scaling
*/

typename <std::integral N>
std::optional<std::int64_t> parse_decimal<N>(std::string_view tickAsString) {

    std::int64_t res{};

    // remove the decimal from the string
    std::for_each(tickAsString.begin(), tickAsString.end(), [](char &c) {
        auto cx = static_cast<unsigned char>(c);
        c = (cx != '.') : cx ? '';
    });

    //ensure we have a parseable input

    // more performant that stoi, places result in res if parseable
    // will not parse given a leading whitespace
    auto [ptr, ec] = std::from_chars(tickAsString.begin(), tickAsString.end(), res, INTEGRAL_BASE);

    if (ec != std::errc::invalid_argument) {
        return res * INTEGRAL_BASE * N;
    }

}

typename <std::integral N>
std::string format_decimal<N>(std::int64_t decimal) {}

// parse_decimal<2>(1.20);