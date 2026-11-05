#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "types.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

// ===========================================================================
// Compile-time contract tests
//
// These encode design invariants. They fail to BUILD if a rule is broken,
// which is strictly stronger than a runtime CHECK: the compiler enforces them
// on every translation unit that includes the header.
// ===========================================================================
namespace {

// Detection helpers: a requires-expression outside a template is ill-formed if
// its requirements are invalid, so wrap them in templates to get SFINAE-style
// "is this expression valid?" booleans.
template <typename A, typename B>
constexpr bool can_multiply = requires(A a, B b) { a * b; };

template <typename A, typename B>
constexpr bool can_compare_equal = requires(A a, B b) { a == b; };

// The wrappers must be distinct, non-interchangeable types.
static_assert(!std::is_same_v<Price, Quantity>);
static_assert(!std::is_same_v<Price, Funding>);
static_assert(!std::is_same_v<Price, Notional>);

// Zero-overhead: one int64, no vptr, trivially copyable.
static_assert(sizeof(Price) == sizeof(std::int64_t));
static_assert(sizeof(Quantity) == sizeof(std::int64_t));
static_assert(sizeof(Funding) == sizeof(std::int64_t));
static_assert(sizeof(Notional) == sizeof(std::int64_t));
static_assert(std::is_trivially_copyable_v<Price>);
static_assert(std::is_trivially_copyable_v<Quantity>);

// Construction is explicit: no implicit int -> wrapper conversion...
static_assert(!std::is_convertible_v<std::int64_t, Price>);
static_assert(!std::is_convertible_v<std::int64_t, Quantity>);
static_assert(!std::is_convertible_v<std::int64_t, Funding>);
static_assert(!std::is_convertible_v<std::int64_t, Notional>);
// ...but explicit construction is allowed.
static_assert(std::is_constructible_v<Price, std::int64_t>);

// No default construction: every value must be supplied (no "empty" price).
static_assert(!std::is_default_constructible_v<Price>);

// Price - Price yields a raw signed tick delta, not a Price.
static_assert(std::is_same_v<decltype(std::declval<Price>() - std::declval<Price>()),
                             std::int64_t>);

// Multiplication exists in exactly one direction and yields a Notional.
static_assert(std::is_same_v<decltype(std::declval<Price>() * std::declval<Quantity>()),
                             Notional>);
static_assert(can_multiply<Price, Quantity>);
static_assert(!can_multiply<Quantity, Price>);   // reverse order forbidden
static_assert(!can_multiply<Price, Price>);      // price * price is nonsense
static_assert(!can_compare_equal<Price, Quantity>);  // no cross-type comparison

// The three scales must compose: 1e-4 dollars * 1e-2 contracts == 1e-6 dollars.
static_assert(PRICE_SCALE * QUANTITY_SCALE == FUNDING_SCALE);

}  // namespace

// ===========================================================================
// Runtime tests
// ===========================================================================

TEST_CASE("Price is a signed, totally ordered value") {
    CHECK(Price{-100} < Price{0});
    CHECK(Price{-1} < Price{0});
    CHECK(Price{0} < Price{1});
    CHECK(Price{-5} <= Price{-5});
    CHECK(Price{-5} >= Price{-5});
    CHECK(Price{-5} > Price{-6});
    CHECK(Price{7} == Price{7});
    CHECK(Price{7} != Price{8});
    CHECK(Price{std::numeric_limits<std::int64_t>::min()} <
          Price{std::numeric_limits<std::int64_t>::max()});
}

TEST_CASE("ticks() returns the exact integer, including extremes, on const") {
    CHECK_EQ(Price{-1}.ticks(), -1);
    CHECK_EQ(Price{std::numeric_limits<std::int64_t>::max()}.ticks(),
             std::numeric_limits<std::int64_t>::max());
    CHECK_EQ(Price{std::numeric_limits<std::int64_t>::min()}.ticks(),
             std::numeric_limits<std::int64_t>::min());

    const Price p{42};
    CHECK_EQ(p.ticks(), 42);  // must be callable through a const reference
}

TEST_CASE("Price subtraction is a signed delta (would explode if unsigned)") {
    CHECK_EQ(Price{100} - Price{250}, -150);
    CHECK_EQ(Price{250} - Price{100}, 150);
    CHECK_EQ(Price{-50} - Price{50}, -100);
    CHECK_EQ(Price{42} - Price{42}, 0);
}

TEST_CASE("Quantity is signed and ordered") {
    CHECK(Quantity{100} > Quantity{50});
    CHECK(Quantity{0} < Quantity{1});
    CHECK_EQ(Quantity{100} - Quantity{150}, -50);
    CHECK_EQ(Quantity{100}.ticks(), 100);
}

TEST_CASE("Funding is a signed micro-rate") {
    CHECK_EQ(Funding{1}.ticks(), 1);   // +1e-6
    CHECK(Funding{-1} < Funding{0});   // negative funding is legal
    CHECK_EQ(Funding{5} - Funding{8}, -3);
}

TEST_CASE("Price * Quantity -> Notional in 1e-6 dollars") {
    // $1.00 (10'000 ticks) * 1 contract (100 units) == $1.00 == 1'000'000 units @ 1e-6
    CHECK_EQ((Price{10'000} * Quantity{100}).ticks(), 1'000'000);

    // Smallest price tick * smallest quantity tick == one 1e-6 dollar unit.
    CHECK_EQ((Price{1} * Quantity{1}).ticks(), 1);

    // Non-round case: $1.2345 * 2.50 contracts == $3.08625
    CHECK_EQ((Price{12'345} * Quantity{250}).ticks(), 3'086'250);

    // Signed operands flow through the product correctly.
    CHECK_EQ((Price{-10'000} * Quantity{100}).ticks(), -1'000'000);
    CHECK_EQ((Price{-10'000} * Quantity{-100}).ticks(), 1'000'000);
}

TEST_CASE("Price * Quantity stays exact just below the int64 ceiling") {
    // 3e9 * 3e9 == 9e18, comfortably under INT64_MAX (9.223e18).
    CHECK_EQ((Price{3'000'000'000LL} * Quantity{3'000'000'000LL}).ticks(),
             9'000'000'000'000'000'000LL);
}

TEST_CASE("MarketSnapshot carries optional book data plus required funding") {
    MarketSnapshot s{
        .bid = Price{8'700},
        .ask = Price{8'800},
        .last = Price{8'750},
        .funding = Funding{3},
        .snapshot_time = 1'700'000'000'000'000'000ULL,
    };
    REQUIRE(s.bid.has_value());
    REQUIRE(s.ask.has_value());
    REQUIRE(s.last.has_value());
    CHECK(*s.bid <= *s.ask);
    CHECK_EQ(s.bid->ticks(), 8'700);
    CHECK_EQ(s.funding.ticks(), 3);
    CHECK_EQ(s.snapshot_time, 1'700'000'000'000'000'000ULL);
}

TEST_CASE("empty and one-sided books are representable") {
    const MarketSnapshot empty{std::nullopt, std::nullopt, std::nullopt, Funding{0}, 0};
    CHECK_FALSE(empty.bid.has_value());
    CHECK_FALSE(empty.ask.has_value());
    CHECK_FALSE(empty.last.has_value());
    CHECK(empty.bid == std::nullopt);
    CHECK(empty.funding == Funding{0});  // funding is never absent

    const MarketSnapshot bid_only{Price{5}, std::nullopt, std::nullopt, Funding{0}, 0};
    REQUIRE(bid_only.bid.has_value());
    CHECK(bid_only.bid == Price{5});
    CHECK_FALSE(bid_only.ask.has_value());
}

TEST_CASE("Order carries side, ids, typed price and quantity") {
    Order o{Side::buy, 42, Price{8'700}, Quantity{100}, 1'700'000'000'000'000'000ULL};
    CHECK(o.side == Side::buy);
    CHECK_EQ(o.order_id, 42u);
    CHECK_EQ(o.price.ticks(), 8'700);
    CHECK_EQ(o.quantity.ticks(), 100);
}

// ---------------------------------------------------------------------------
// Known holes. These document defects; they are NOT endorsements.
// When the production code gains the missing guard, invert/delete each test.
// ---------------------------------------------------------------------------

TEST_CASE("GAP: MarketSnapshot does not enforce bid <= ask") {
    // Nothing in the type prevents a crossed book. This test will start
    // failing (good) once validation is added at construction/ingestion.
    const MarketSnapshot crossed{Price{200}, Price{100}, std::nullopt, Funding{0}, 0};
    REQUIRE(crossed.bid.has_value());
    REQUIRE(crossed.ask.has_value());
    CHECK(*crossed.bid > *crossed.ask);  // documenting that the type allows it
}

TEST_CASE("GAP: operator* has no overflow guard") {
    // Price{4e9} * Quantity{4e9} == 1.6e19, beyond INT64_MAX. There is no check,
    // so this would silently overflow (undefined behaviour). Not executed here.
    // Fix: a checked multiply that returns std::optional<Notional> / throws.
    CHECK(true);  // placeholder so the gap is visible by name
}