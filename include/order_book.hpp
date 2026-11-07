#pragma once

#include "types.hpp"

#include <map>
#include <span>
#include <optional>
#include <cstddef>
#include <cstdint>

// Price = 1e-4 ticks
// Level Qty = std::int64_t at 1e-2 units, hundrethds of a contract

struct PriceLevel {
    Price price;
    std::int64_t qty;
};

struct SnapshotStats {
    std::size_t accepted{};
    std::size_t rejected_nonpositive{};
    std::size_t duplicate_prices{};
};

class OrderBook {

public:
    SnapshotStats apply_snapshot(Side side, std::span<const PriceLevel> levels) {

        SnapshotStats res{};
        std::map<Price, std::int64_t> fresh;

        for (const PriceLevel& level : levels) {
            if (level.qty <= 0) {
                res.rejected_nonpositive++;
                continue;
            }

            auto [it, inserted] = fresh.insert_or_assign(level.price, level.qty);
            inserted ? ++res.accepted : ++res.duplicate_prices;
        }

        map_for(side).swap(fresh);

        return res;
        

    }

    std::optional<Price> best_bid() const {
        if (!bids_.empty()) {
            return bids_.rbegin()->first;
        }

        return std::nullopt;
    }

    std::optional<Price> best_ask() const {
        if (!asks_.empty()) {
            return asks_.begin()->first;
        }
        return std::nullopt;
    }

    bool is_crossed() const {
        
        // empty books cannot cross
        if (!level_count(Side::buy) || !level_count(Side::sell)) {
            return false;
        }

        return (best_bid() >= best_ask());
    }

    std::size_t level_count(Side side) const {
        return(map_for(side).size());
    }

    std::int64_t qty_at(Side side, Price price) const {

        auto& map = map_for(side);
        auto mapIt = map.find(price);

        //const std::int64_t res = (mapIt == map.end() ? 0: mapIt->second);

        return (mapIt == map.end() ? 0: mapIt->second);
    }
    void clear() {
        bids_.clear();
        asks_.clear();
    }

private:

    std::map<Price, std::int64_t>& map_for(Side side) {
        return side == Side::buy ? bids_ : asks_;
    }

    const std::map<Price, std::int64_t>& map_for(Side side) const { 
        return side == Side::buy ? bids_ : asks_;
    }

    std::map<Price, std::int64_t> bids_;
    std::map<Price, std::int64_t> asks_;
};
