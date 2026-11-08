#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "order_book.hpp"

#include <algorithm>
#include <vector>

namespace {

Price at(std::int64_t ticks) { return Price{ticks}; }

std::span<const PriceLevel> span_of(const std::vector<PriceLevel>& v) {
    return {v.data(), v.size()};
}

std::size_t partition(const SnapshotStats& s) {
    return s.accepted + s.rejected_nonpositive + s.duplicate_prices;
}

} // namespace

// ---------------------------------------------------------------- basics

TEST_CASE("empty book: no prices, not crossed, zero levels") {
    OrderBook b;
    CHECK_FALSE(b.best_bid().has_value());
    CHECK_FALSE(b.best_ask().has_value());
    CHECK_FALSE(b.is_crossed());
    CHECK_EQ(b.level_count(Side::buy), 0u);
    CHECK_EQ(b.level_count(Side::sell), 0u);
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 0);
    CHECK_EQ(b.qty_at(Side::sell, at(75805)), 0);
}

TEST_CASE("snapshot load: best bid is the highest bid, best ask the lowest ask") {
    OrderBook b;

    std::vector<PriceLevel> bids{{at(75801), 100}, {at(75805), 120}, {at(75803), 300}};
    std::vector<PriceLevel> asks{{at(75809), 95}, {at(75811), 250}, {at(75810), 40}};

    auto rb = b.apply_snapshot(Side::buy, span_of(bids));
    auto ra = b.apply_snapshot(Side::sell, span_of(asks));

    REQUIRE(b.best_bid().has_value());
    REQUIRE(b.best_ask().has_value());
    CHECK_EQ(b.best_bid()->ticks(), 75805);   // highest, NOT lowest
    CHECK_EQ(b.best_ask()->ticks(), 75809);   // lowest, NOT highest

    CHECK_EQ(b.level_count(Side::buy), 3u);
    CHECK_EQ(b.level_count(Side::sell), 3u);
    CHECK_EQ(rb.accepted, 3u);
    CHECK_EQ(ra.accepted, 3u);

    CHECK_EQ(b.qty_at(Side::buy, at(75801)), 100);
    CHECK_EQ(b.qty_at(Side::buy, at(75803)), 300);
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 120);
    CHECK_EQ(b.qty_at(Side::sell, at(75811)), 250);

    CHECK_FALSE(b.is_crossed());
}

TEST_CASE("qty_at returns 0 for absent prices and for the wrong side") {
    OrderBook b;
    std::vector<PriceLevel> bids{{at(75805), 120}};
    b.apply_snapshot(Side::buy, span_of(bids));

    CHECK_EQ(b.qty_at(Side::buy, at(75800)), 0);
    CHECK_EQ(b.qty_at(Side::buy, at(99999)), 0);
    CHECK_EQ(b.qty_at(Side::sell, at(75805)), 0);   // present on bid side only
}

// ------------------------------------------------------- replace semantics

TEST_CASE("apply_snapshot replaces: levels absent from the new snapshot are gone") {
    OrderBook b;

    std::vector<PriceLevel> first{{at(75805), 120}, {at(75803), 300}};
    b.apply_snapshot(Side::buy, span_of(first));
    REQUIRE_EQ(b.level_count(Side::buy), 2u);

    std::vector<PriceLevel> second{{at(75801), 50}};
    auto r = b.apply_snapshot(Side::buy, span_of(second));

    CHECK_EQ(b.level_count(Side::buy), 1u);
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 0);    // stale levels must not survive
    CHECK_EQ(b.qty_at(Side::buy, at(75803)), 0);
    CHECK_EQ(b.qty_at(Side::buy, at(75801)), 50);
    CHECK_EQ(r.accepted, 1u);                       // merge would have reported 1 too...
    CHECK_EQ(b.best_bid()->ticks(), 75801);         // ...so the level count is the real check
}

TEST_CASE("apply_snapshot does not touch the opposite side") {
    OrderBook b;
    std::vector<PriceLevel> bids_{{at(75805), 120}};
    std::vector<PriceLevel> asks_{{at(75809), 95}};
    b.apply_snapshot(Side::buy, span_of(bids_));
    b.apply_snapshot(Side::sell, span_of(asks_));

    std::vector<PriceLevel> newBids{{at(75801), 10}};
    b.apply_snapshot(Side::buy, span_of(newBids));

    CHECK_EQ(b.qty_at(Side::sell, at(75809)), 95);  // untouched
    CHECK_EQ(b.level_count(Side::sell), 1u);
}

TEST_CASE("empty snapshot empties that side, and an all-rejected snapshot does too") {
    OrderBook b;
    std::vector<PriceLevel> bids_{{at(75805), 120}, {at(75803), 300}};
    b.apply_snapshot(Side::buy, span_of(bids_));

    std::vector<PriceLevel> none;
    auto r = b.apply_snapshot(Side::buy, span_of(none));
    CHECK_EQ(b.level_count(Side::buy), 0u);
    CHECK_FALSE(b.best_bid().has_value());
    CHECK_EQ(r.accepted, 0u);

    b.apply_snapshot(Side::buy, span_of(bids_));
    std::vector<PriceLevel> allBad{{at(75805), 0}, {at(75803), -5}};
    auto r2 = b.apply_snapshot(Side::buy, span_of(allBad));
    CHECK_EQ(b.level_count(Side::buy), 0u);          // replace-with-nothing, not a no-op
    CHECK_EQ(r2.rejected_nonpositive, 2u);
    CHECK_EQ(r2.accepted, 0u);
}

TEST_CASE("applying the same snapshot twice is idempotent") {
    OrderBook b;
    std::vector<PriceLevel> snap{{at(75805), 120}, {at(75803), 300}};

    auto r1 = b.apply_snapshot(Side::buy, span_of(snap));
    auto r2 = b.apply_snapshot(Side::buy, span_of(snap));

    CHECK_EQ(r1.accepted, 2u);
    CHECK_EQ(r2.accepted, 2u);          // a replace, so still 2 accepted, 0 duplicates
    CHECK_EQ(r2.duplicate_prices, 0u);  // duplicates are per-call, not "already in the book"
    CHECK_EQ(b.level_count(Side::buy), 2u);
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 120);
}

// ------------------------------------------------------------- counters

TEST_CASE("non-positive quantities are rejected, counted, and never stored") {
    OrderBook b;
    std::vector<PriceLevel> snap{{at(75805), 120}, {at(75803), 0}, {at(75801), -7}};
    auto r = b.apply_snapshot(Side::buy, span_of(snap));

    CHECK_EQ(r.accepted, 1u);
    CHECK_EQ(r.rejected_nonpositive, 2u);
    CHECK_EQ(r.duplicate_prices, 0u);
    CHECK_EQ(b.level_count(Side::buy), 1u);
    CHECK_EQ(b.qty_at(Side::buy, at(75803)), 0);
    CHECK_EQ(b.qty_at(Side::buy, at(75801)), 0);
    CHECK_EQ(partition(r), snap.size());
}

TEST_CASE("duplicate price: last valid quantity wins, and is counted") {
    OrderBook b;
    std::vector<PriceLevel> snap{{at(75805), 120}, {at(75805), 6600}};
    auto r = b.apply_snapshot(Side::buy, span_of(snap));

    CHECK_EQ(r.accepted, 1u);
    CHECK_EQ(r.duplicate_prices, 1u);
    CHECK_EQ(b.level_count(Side::buy), 1u);
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 6600);
    CHECK_EQ(partition(r), snap.size());
}

TEST_CASE("a rejected duplicate must not clobber the valid level") {
    OrderBook b;
    std::vector<PriceLevel> snap{{at(75805), 120}, {at(75805), -1}, {at(75805), 0}};
    auto r = b.apply_snapshot(Side::buy, span_of(snap));

    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 120);  // last *valid* wins
    CHECK_EQ(r.accepted, 1u);
    CHECK_EQ(r.rejected_nonpositive, 2u);
    CHECK_EQ(r.duplicate_prices, 0u);
    CHECK_EQ(partition(r), snap.size());
}

TEST_CASE("three copies of one price: one accepted level, two duplicates") {
    OrderBook b;
    std::vector<PriceLevel> snap{{at(75805), 10}, {at(75805), 20}, {at(75805), 30}};
    auto r = b.apply_snapshot(Side::buy, span_of(snap));

    CHECK_EQ(r.accepted, 1u);
    CHECK_EQ(r.duplicate_prices, 2u);   // duplicates may exceed accepted
    CHECK_EQ(b.level_count(Side::buy), 1u);
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 30);
    CHECK_EQ(partition(r), snap.size());
}

TEST_CASE("counters partition the input and agree with observable state") {
    // mix of valid, duplicate, zero and negative quantities
    std::vector<PriceLevel> snap{
        {at(75805), 120}, {at(75803), 300}, {at(75805), 6600},
        {at(75801), 0},   {at(75803), -100}, {at(75809), 5},
        {at(75809), 6},   {at(75799), 42},
    };
    OrderBook b;
    auto r = b.apply_snapshot(Side::buy, span_of(snap));

    CHECK_EQ(partition(r), snap.size());
    CHECK_EQ(r.accepted, b.level_count(Side::buy));
    CHECK_EQ(r.rejected_nonpositive, 2u);
    CHECK_EQ(r.duplicate_prices, 2u);
    CHECK_EQ(r.accepted, 4u);
}

TEST_CASE("unsorted input produces identical state to sorted input") {
    std::vector<PriceLevel> a{{at(75801), 10}, {at(75809), 90}, {at(75805), 50}, {at(75803), 30}};
    std::vector<PriceLevel> b{a};
    std::sort(b.begin(), b.end(), [](const PriceLevel& x, const PriceLevel& y) {
        return x.price.ticks() < y.price.ticks();
    });

    OrderBook ba, bb;
    ba.apply_snapshot(Side::buy, span_of(a));
    bb.apply_snapshot(Side::buy, span_of(b));

    CHECK_EQ(ba.level_count(Side::buy), bb.level_count(Side::buy));
    CHECK_EQ(ba.best_bid()->ticks(), bb.best_bid()->ticks());
    for (std::int64_t p : {75801, 75803, 75805, 75809}) {
        CHECK_EQ(ba.qty_at(Side::buy, at(p)), bb.qty_at(Side::buy, at(p)));
    }
}

// ------------------------------------------------------------- crossing

TEST_CASE("is_crossed: empty and one-sided books are never crossed") {
    OrderBook empty;
    CHECK_FALSE(empty.is_crossed());

    OrderBook bidOnly;
    std::vector<PriceLevel> bids_{{at(75805), 120}};
    bidOnly.apply_snapshot(Side::buy, span_of(bids_));
    CHECK_FALSE(bidOnly.is_crossed());

    OrderBook askOnly;
    std::vector<PriceLevel> asks_{{at(75805), 120}};
    askOnly.apply_snapshot(Side::sell, span_of(asks_));
    CHECK_FALSE(askOnly.is_crossed());
}

TEST_CASE("is_crossed: locked and crossed books are detected") {
    std::vector<PriceLevel> bids_{{at(75805), 120}};
    std::vector<PriceLevel> asks_{};

    OrderBook b;
    b.apply_snapshot(Side::buy, span_of(bids_));
    asks_ = {{at(75809), 95}};
    b.apply_snapshot(Side::sell, span_of(asks_));
    CHECK_FALSE(b.is_crossed());

    asks_ = {{at(75805), 95}};                       // locked
    b.apply_snapshot(Side::sell, span_of(asks_));
    CHECK(b.is_crossed());

    asks_ = {{at(75801), 95}};                       // crossed
    b.apply_snapshot(Side::sell, span_of(asks_));
    CHECK(b.is_crossed());
}

// --------------------------------------------------------------- misc

TEST_CASE("clear empties both sides") {
    OrderBook b;
    std::vector<PriceLevel> bids_{{at(75805), 120}};
    std::vector<PriceLevel> asks_{{at(75809), 95}};
    b.apply_snapshot(Side::buy, span_of(bids_));
    b.apply_snapshot(Side::sell, span_of(asks_));

    b.clear();

    CHECK_EQ(b.level_count(Side::buy), 0u);
    CHECK_EQ(b.level_count(Side::sell), 0u);
    CHECK_FALSE(b.best_bid().has_value());
    CHECK_FALSE(b.best_ask().has_value());
}

TEST_CASE("large snapshot: 10k levels per side keeps best prices correct") {
    std::vector<PriceLevel> bids_, asks_;
    bids_.reserve(10000);
    asks_.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        bids_.push_back({at(100000 + i), 100 + i});
        asks_.push_back({at(200000 + i), 200 + i});
    }

    OrderBook b;
    auto rb = b.apply_snapshot(Side::buy, span_of(bids_));
    auto ra = b.apply_snapshot(Side::sell, span_of(asks_));

    CHECK_EQ(rb.accepted, 10000u);
    CHECK_EQ(ra.accepted, 10000u);
    CHECK_EQ(b.level_count(Side::buy), 10000u);
    CHECK_EQ(b.best_bid()->ticks(), 109999);   // highest
    CHECK_EQ(b.best_ask()->ticks(), 200000);   // lowest
    CHECK_EQ(b.qty_at(Side::buy, at(100000)), 100);
    CHECK_EQ(b.qty_at(Side::sell, at(209999)), 10199);
    CHECK_FALSE(b.is_crossed());
}

// ======================================================= apply_delta (CPP-42)

namespace {
bool erased(ApplyResult r) { return r == ApplyResult::level_erased; }
bool applied(ApplyResult r) { return r == ApplyResult::applied; }
} // namespace

TEST_CASE("apply_delta: positive delta creates a level at an absent price") {
    OrderBook b;
    auto r = b.apply_delta(Side::buy, at(75801), 50);
    CHECK(applied(r));
    CHECK_EQ(b.level_count(Side::buy), 1u);
    CHECK_EQ(b.qty_at(Side::buy, at(75801)), 50);
    CHECK_EQ(b.best_bid()->ticks(), 75801);
}

TEST_CASE("apply_delta: positive delta adds to an existing level") {
    OrderBook b;
    std::vector<PriceLevel> s{{at(75805), 200}};
    b.apply_snapshot(Side::buy, span_of(s));

    auto r = b.apply_delta(Side::buy, at(75805), 25);
    CHECK(applied(r));
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 225);
    CHECK_EQ(b.level_count(Side::buy), 1u);
}

TEST_CASE("apply_delta: negative delta subtracts (does not add)") {
    OrderBook b;
    std::vector<PriceLevel> s{{at(75805), 200}};
    b.apply_snapshot(Side::buy, span_of(s));

    auto r = b.apply_delta(Side::buy, at(75805), -100);
    CHECK(applied(r));
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 100);   // 200 - 100, not 200 + 100
    CHECK_EQ(b.level_count(Side::buy), 1u);
}

TEST_CASE("apply_delta: negative delta exactly equal to the level erases it") {
    OrderBook b;
    std::vector<PriceLevel> s{{at(75803), 100}, {at(75805), 200}};
    b.apply_snapshot(Side::buy, span_of(s));

    auto r = b.apply_delta(Side::buy, at(75805), -200);
    CHECK(erased(r));
    CHECK_EQ(b.level_count(Side::buy), 1u);
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 0);
    CHECK_EQ(b.best_bid()->ticks(), 75803);          // best follows the erase
}

TEST_CASE("apply_delta: erasing the top level repeatedly walks the best price down") {
    OrderBook b;
    std::vector<PriceLevel> s{{at(75801), 10}, {at(75803), 20}, {at(75805), 30}};
    b.apply_snapshot(Side::buy, span_of(s));

    CHECK(erased(b.apply_delta(Side::buy, at(75805), -30)));
    CHECK_EQ(b.best_bid()->ticks(), 75803);
    CHECK(erased(b.apply_delta(Side::buy, at(75803), -20)));
    CHECK_EQ(b.best_bid()->ticks(), 75801);
    CHECK(erased(b.apply_delta(Side::buy, at(75801), -10)));
    CHECK_EQ(b.level_count(Side::buy), 0u);
    CHECK_FALSE(b.best_bid().has_value());           // empty side -> nullopt
}

TEST_CASE("apply_delta: overshooting a level is rejected and changes nothing") {
    OrderBook b;
    std::vector<PriceLevel> s{{at(75803), 100}, {at(75805), 200}};
    b.apply_snapshot(Side::buy, span_of(s));

    auto r = b.apply_delta(Side::buy, at(75805), -5000);
    CHECK(r == ApplyResult::would_go_negative);
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 200);   // untouched
    CHECK_EQ(b.level_count(Side::buy), 2u);
    CHECK_EQ(b.best_bid()->ticks(), 75805);
}

TEST_CASE("apply_delta: negative delta at an absent price is rejected, book unchanged") {
    OrderBook b;
    std::vector<PriceLevel> s{{at(75805), 200}};
    b.apply_snapshot(Side::buy, span_of(s));

    auto r = b.apply_delta(Side::buy, at(70000), -5);
    CHECK(r == ApplyResult::unknown_level_negative);
    CHECK_EQ(b.level_count(Side::buy), 1u);          // no phantom level
    CHECK_EQ(b.qty_at(Side::buy, at(70000)), 0);
}

TEST_CASE("apply_delta: zero delta is a no-op that never creates a level") {
    OrderBook b;
    std::vector<PriceLevel> s{{at(75805), 200}};
    b.apply_snapshot(Side::buy, span_of(s));

    auto r1 = b.apply_delta(Side::buy, at(75805), 0);   // present
    CHECK(applied(r1));
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 200);

    auto r2 = b.apply_delta(Side::buy, at(70000), 0);   // absent
    CHECK(applied(r2));                                 // not "negative"
    CHECK_EQ(b.level_count(Side::buy), 1u);             // still no zero-qty level
}

TEST_CASE("apply_delta: INT64_MIN delta must not invoke UB") {
    OrderBook b;
    std::vector<PriceLevel> s{{at(75805), 100}};
    b.apply_snapshot(Side::buy, span_of(s));

    const std::int64_t worst = std::numeric_limits<std::int64_t>::min();
    auto r = b.apply_delta(Side::buy, at(75805), worst);
    CHECK(r == ApplyResult::would_go_negative);
    CHECK_EQ(b.qty_at(Side::buy, at(75805)), 100);
}

TEST_CASE("apply_delta: pathological positive delta must not overflow the level") {
    OrderBook b;
    std::vector<PriceLevel> s{{at(75805), 100}};
    b.apply_snapshot(Side::buy, span_of(s));

    auto r = b.apply_delta(Side::buy, at(75805), std::numeric_limits<std::int64_t>::max());
    CHECK((b.qty_at(Side::buy, at(75805)) >= 100));   // no wrap to negative, ever
    (void)r;
}

TEST_CASE("apply_delta: random sequence matches a reference map model") {
    OrderBook b;
    std::map<Price, std::int64_t> ref;

    std::uint64_t seed = 12345;
    auto rnd = [&seed]() { seed = seed * 6364136223846793005ULL + 1442695040888963407ULL; return seed >> 33; };

    for (int i = 0; i < 4000; ++i) {
        const std::int64_t px = 75800 + static_cast<std::int64_t>(rnd() % 12);
        const std::int64_t delta = static_cast<std::int64_t>(rnd() % 401) - 200;  // -200..200

        const auto it = ref.find(at(px));
        const bool present = it != ref.end();
        const std::int64_t cur = present ? it->second : 0;

        auto got = b.apply_delta(Side::buy, at(px), delta);

        if (delta > 0) {
            ref[at(px)] = cur + delta;
            CHECK(applied(got));
        } else if (delta == 0) {
            CHECK(applied(got));
            CHECK_EQ(present, ref.count(at(px)) > 0);
        } else if (!present) {
            CHECK(got == ApplyResult::unknown_level_negative);
        } else if (-delta > cur) {
            CHECK(got == ApplyResult::would_go_negative);
        } else if (-delta == cur) {
            ref.erase(it);
            CHECK(erased(got));
        } else {
            ref[at(px)] = cur + delta;
            CHECK(applied(got));
        }

        REQUIRE_EQ(b.level_count(Side::buy), ref.size());
        for (const auto& [p, q] : ref) {
            REQUIRE_EQ(b.qty_at(Side::buy, p), q);
        }
        REQUIRE_EQ(b.qty_at(Side::buy, at(px)), ref.count(at(px)) ? ref.at(at(px)) : 0);
    }

    if (ref.empty()) {
        CHECK_FALSE(b.best_bid().has_value());
    } else {
        CHECK_EQ(b.best_bid()->ticks(), ref.rbegin()->first.ticks());
    }
}
