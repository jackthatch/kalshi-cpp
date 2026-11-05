#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <limits>

constexpr std::uint64_t PRICE_SCALE {10'000};
constexpr std::uint64_t QUANTITY_SCALE {100};
constexpr std::uint64_t FUNDING_SCALE {1'000'000};

enum class Side {
    buy,
    sell
};

struct Price {
    explicit Price(std::int64_t ticks) : ticks_(ticks) {}

    // comparisons
    friend bool operator==(Price a, Price b) { return a.ticks_ == b.ticks_; };
    friend bool operator<(Price a, Price b) { return a.ticks_ < b.ticks_; };
    friend bool operator<=(Price a, Price b) { return a.ticks_ <= b.ticks_; };
    friend bool operator>(Price a, Price b) { return a.ticks_ > b.ticks_; };
    friend bool operator>=(Price a, Price b) { return a.ticks_ >= b.ticks_; };


    // arithmetic
    friend std::int64_t operator-(Price a, Price b) { return a.ticks_ - b.ticks_; };

    // functions
    std::int64_t ticks() const { return ticks_; }

private:
    std::int64_t ticks_;
};

struct Quantity {
    explicit Quantity(std::int64_t ticks) : ticks_(ticks) {}

    // comparisons
    friend bool operator==(Quantity a, Quantity b) { return a.ticks_ == b.ticks_; };
    friend bool operator<(Quantity a, Quantity b) { return a.ticks_ < b.ticks_; };
    friend bool operator<=(Quantity a, Quantity b) { return a.ticks_ <= b.ticks_; };
    friend bool operator>(Quantity a, Quantity b) { return a.ticks_ > b.ticks_; };
    friend bool operator>=(Quantity a, Quantity b) { return a.ticks_ >= b.ticks_; };

    // arithmetic
    friend std::int64_t operator-(Quantity a, Quantity b) { return a.ticks_ - b.ticks_; };

    // functions
    std::int64_t ticks() const { return ticks_; }

private:
    std::int64_t ticks_;
};

struct Funding {
    explicit Funding(std::int64_t ticks) : ticks_(ticks) {}

    // comparisons
    friend bool operator==(Funding a, Funding b) { return a.ticks_ == b.ticks_; };
    friend bool operator<(Funding a, Funding b) { return a.ticks_ < b.ticks_; };
    friend bool operator<=(Funding a, Funding b) { return a.ticks_ <= b.ticks_; };
    friend bool operator>(Funding a, Funding b) { return a.ticks_ > b.ticks_; };
    friend bool operator>=(Funding a, Funding b) { return a.ticks_ >= b.ticks_; };

    // arithmetic
    friend std::int64_t operator-(Funding a, Funding b) { return a.ticks_ - b.ticks_; };

    // functions
    std::int64_t ticks() const { return ticks_; }

private:
    std::int64_t ticks_;
};

struct Notional {

    Notional() : ticks_(0) {}
    explicit Notional(std::int64_t ticks) : ticks_(ticks) {}

    // comparisons
    friend bool operator==(Notional a, Notional b) { return a.ticks_ == b.ticks_; };
    friend bool operator<(Notional a, Notional b) { return a.ticks_ < b.ticks_; };
    friend bool operator<=(Notional a, Notional b) { return a.ticks_ <= b.ticks_; };
    friend bool operator>(Notional a, Notional b) { return a.ticks_ > b.ticks_; };
    friend bool operator>=(Notional a, Notional b) { return a.ticks_ >= b.ticks_; };

    // arithmetic
    friend std::int64_t operator-(Notional a, Notional b) { return a.ticks_ - b.ticks_; };

    // functions
    std::int64_t ticks() const { return ticks_; }
    

private:
    std::int64_t ticks_;
};



struct MarketSnapshot {
    std::optional<Price> bid;
    std::optional<Price> ask;
    std::optional<Price> last;
    Funding funding;
    std::uint64_t snapshot_time;     //ns since epoch
};

struct Order {
    Side side;
    std::uint64_t order_id;
    Price price;
    Quantity quantity;
    std::uint64_t order_time;    // ns since epoch
};



inline std::optional<Notional> operator*(Price a, Quantity b) {

    std::int64_t aTicks {a.ticks()};
    std::int64_t bTicks {b.ticks()};
    int64_t res{};

    if (__builtin_mul_overflow(aTicks, bTicks, &res)) {
        return std::nullopt;
    }

    return Notional{res};
};

