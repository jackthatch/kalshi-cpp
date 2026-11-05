#pragma once

#include <chrono>
#include <cstdint>

constexpr uint64_t PRICE_SCALE {10'000};
constexpr uint64_t QUANTITY_SCALE {100};
constexpr uint64_t FUNDING_SCALE {1'000'000};

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
    friend int64_t operator-(Price a, Price b) { return a.ticks_ - b.ticks_; };

    // functions
    std::int64_t ticks() { return ticks_; }

    
private:
    int64_t ticks_;
};

struct MarketSnapshot {
    std::optional<uint64_t> bid;
    std::optional<uint64_t> ask;
    std::optional<uint64_t> last;
    int64_t funding;
    uint64_t snapshot_time;     //ns since epoch
};

struct Order {
    Side side;
    uint64_t order_id;
    Price price;
    uint64_t quantity;
    uint64_t order_time;    // ns since epoch
};