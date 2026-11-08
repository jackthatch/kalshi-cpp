#pragma once

#include "types.hpp"

#include <map>
#include <span>
#include <optional>
#include <cstddef>
#include <cstdint>

// Price = 1e-4 ticks
// Level Qty = std::int64_t at 1e-2 units, hundredths of a contract

struct PriceLevel {
    Price price;
    std::int64_t qty;
};

struct SnapshotStats {
    std::size_t accepted{};
    std::size_t rejected_nonpositive{};
    std::size_t duplicate_prices{};
};

enum class ApplyResult {
    applied,
    level_erased,
    unknown_level_negative, // negative delta at a price we don't have
    would_go_negative,       // |delta| > current level qty
    overflow                // operation would overflow the qty at the pricelevel, a bit clearer result that would_go_negative
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

    ApplyResult apply_delta(Side side, Price price, std::int64_t delta) {
        auto& map = map_for(side);

        auto priceIt = map.find(price);
        auto priceInBook = priceIt != map.end();

        if (delta > 0) {
            if(priceInBook) {
                std::int64_t res {};
                if(__builtin_add_overflow(priceIt->second, delta, &res)) {
                    return ApplyResult{ApplyResult::overflow};
                }
                priceIt->second = res;
            } else {
                map[price] = delta;
            }
            return ApplyResult{ApplyResult::applied};
        }

        if (delta == 0) {
            return ApplyResult{ApplyResult::applied};
        } 


        if (priceInBook) {
            // std::abs(int64_t) is UB so we need to handle this properly
            const auto mag = static_cast<std::uint64_t>(0) - static_cast<std::uint64_t>(delta);  // |delta| safely
            const auto cur = static_cast<std::uint64_t>(priceIt->second);
            if (mag < cur) {
                std::int64_t res {};
                if(__builtin_add_overflow(cur, delta, &res)) {
                    return ApplyResult{ApplyResult::overflow};
                }
                priceIt->second = res;
                return ApplyResult{ApplyResult::applied};
            }
            if (mag == cur) {
                map.erase(priceIt);
                return ApplyResult{ApplyResult::level_erased};
            } else {
                return ApplyResult{ApplyResult::would_go_negative};
            }
        } 

        // else we don't have the price in our book and it would go negative
        return ApplyResult{ApplyResult::unknown_level_negative};
         


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
