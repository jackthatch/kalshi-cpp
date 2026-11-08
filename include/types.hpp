#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

/*
    scalars we use to conver the kalshi information to a standardized tick to eliminated floating point comparison within the system
    N us the exponent of the scaler eg 10eN
    this is used when safely consuming kalshi information into the system via
    std::optional<std::int64_t> parse_decimal<N>(std::string_view tickAsString)
*/

constexpr std::uint64_t PRICE_SCALE {10'000'000'000};
constexpr std::uint64_t PRICE_N {10};
constexpr std::uint64_t QUANTITY_SCALE {100'000'000};
constexpr std::uint64_t QUANTITY_N {8};
constexpr std::uint64_t FUNDING_SCALE {1'000'000};
constexpr std::uint64_t FUNDING_N {6};

enum class Side {
    buy,
    sell
};

//1e-4
struct Price {
    explicit Price(std::int64_t ticks) : ticks_(ticks) {

    }

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


//1e-2
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

//1e-6
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

//1e-6
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

inline Notional operator*(Price a, Quantity b) {
    return Notional{a.ticks() * b.ticks()};
}
