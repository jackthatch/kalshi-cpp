#pragma once

#include <chrono>
#include <cstdint>

constexpr uint64_t PRICE_SCALE {10'000}
constexpr uint64_t QUANTITY_SCALE {10'000}

enum class Side {
    buy,
    sell
}

struct Price {
    uint64_t price
}

struct MarketSnapshot {
    uint64_t bid;
    uint64_t ask;
    uint64_t funding;
    std::chrono::time_point snapshot_time;
}

class Order {

public:

    // should never create an empty order
    Order() = delete;



private:
    Side side;
    uint64_t order_id;
    uint64_t price;
    uint64_t quantity;
    std::chrono::time_point order_time;
}