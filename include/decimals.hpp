#pragma once

#include "types.hpp"
#include <cstdint>
#include <string_view>
#include <cctype>
#include <iostream>

/*
    Different quantities required different scaling
    Quantity => N == 2 == 1e2 / 10^2 scaling
    Price => N == 4 == 1e4 / 10^4 scaling
    Funding => N == 6 == 1e6  / 10^6 scaling
*/
constexpr std::int64_t POW10[] = {
    1, 10, 100, 1'000, 10'000, 100'000, 1'000'000, 10'000'000,
    100'000'000, 1'000'000'000, 10'000'000'000LL, 100'000'000'000LL,
    1'000'000'000'000LL, 10'000'000'000'000LL, 100'000'000'000'000LL,
    1'000'000'000'000'000LL, 10'000'000'000'000'000LL,
    100'000'000'000'000'000LL, 1'000'000'000'000'000'000LL
};


//parse a string representation of a decimal as an int scaled 10^N
template <int N>
std::optional<std::int64_t> parse_decimal(std::string_view s) {
    static_assert(N >= 0 && N <= 18);
    static_assert(sizeof(POW10) / sizeof(POW10[0]) > N);

    int sign{1};    // will flip to -1 if negative sign found
    size_t idx{};
    size_t len {s.size()};
    std::int64_t accumulated{};     // running total of the scaled int
    std::int64_t fractionalDigits{};    // number of digits past the decimal place
    std::int64_t digits{};

    if (idx < len && s[idx] == '-') {
        sign = -1;
        idx++;
    }

    while (idx < len && std::isdigit(static_cast<unsigned char>(s[idx]))) {
        std::int64_t t{};
        if (__builtin_mul_overflow(accumulated, 10, &t) ||          // guard against multiplcation overflow
            __builtin_add_overflow(t, s[idx] - '0', &accumulated)) {
            return std::nullopt;
        }
        ++digits;
        ++idx;
    }

    if (idx < len && static_cast<unsigned char>(s[idx]) == '.') {
        ++idx;
        while (idx < len && std::isdigit(s[idx])) {
            std::int64_t t{};
            if (__builtin_mul_overflow(accumulated, 10, &t) ||
                __builtin_add_overflow(t, s[idx] - '0', &accumulated)) {
                return std::nullopt;
            }
            ++fractionalDigits;
            ++digits;
            ++idx;
        }
    }

    if (digits == 0) { return std::nullopt; }
    if (idx != len) { return std::nullopt; }

    while (fractionalDigits > 0 && accumulated % 10 == 0) {
        accumulated /= 10;
        fractionalDigits--;
    }

    if (fractionalDigits > N) { return std::nullopt; }

    std::int64_t scaled{};

    if (__builtin_mul_overflow(accumulated, POW10[N - fractionalDigits], &scaled)) {

        return std::nullopt;
    }

    // we made it
    return sign * scaled;

}

template <int N>
std::string format_decimal(std::int64_t ticks) {

    static_assert(N >= 0 && N <= 18);

    const bool negative = ticks < 0;

    const std::uint64_t mag = negative ? 0ULL - static_cast<std::uint64_t>(ticks) : static_cast<std::uint64_t>(ticks);
    
    const std::uint64_t scale = static_cast<std::uint64_t>(POW10[N]);
    const std::uint64_t intPart = mag / scale;
    const std::uint64_t fracPart = mag % scale;

    std::string out;
    if(negative) out.push_back('-');
    out += std::to_string(intPart);

    if constexpr (N > 0) {
        out.push_back('.');
        std::string frac = std::to_string(fracPart);
        if(frac.size() < static_cast<std::size_t>(N)) {
            out.append(static_cast<std::size_t>(N) - frac.size(), '0');
        }
        out += frac;
    }
    return out;
}