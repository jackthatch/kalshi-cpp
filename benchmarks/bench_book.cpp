/*
    Benchmarks for the order_book

    State as of 11/09/2026

                                   small 130lv   large 2000lv
        harness floor              0.27 ns
        qty_at hit                 4.69 ns       6.92 ns      O(log n)   +48%
        qty_at miss                5.24 ns       6.87 ns
        best_qty                   2.20 ns       2.21 ns      O(1)       flat
        level_count                0.25 ns       0.25 ns      O(1)       flat
        is_crossed                10.51 ns      10.55 ns      O(1)       flat
        depth 10                   9.22 ns       9.09 ns      O(k)       flat
        depth 50                  20.32 ns      20.25 ns      O(k)       flat
        depth 200                 54.00 ns      53.69 ns      O(k)       flat
        apply_delta update         5.46 ns                    == qty_at
        create+erase (pair)       35.75 ns                    allocator-bound
                                  (33.9 ... 46.4 ns spread = tcache vs arena)

    Findings
      - depth_within is O(levels scanned), NOT O(book size): 15x more levels,
        identical cost. Marginal ~2.7 ns/level (RB-tree increment). The early
        break works; reproduced across three independent runs (+-0.4%).
      - A write costs the same as a read (update 5.46 vs qty_at hit 4.69):
        same descent, plus a store into a node already hot.
      - create+erase is ~6x update and allocator-dominated. Insert cost is a
        property of heap state, not of the book.
      - depth_within carries a ~7 ns intercept vs best_qty's 2.2 ns: it calls
        best_bid(), which returns std::optional<Price> by value. Reading
        map.rbegin()->first.ticks() directly should halve it.

*/


#include "order_book.hpp"

#include <string>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <utility>

struct BenchResult {
    std::string benchmark_type;     // function name or path, the thing being benchmarked
    std::int64_t iterations;        // iterations run per repetetion
    int repetitions;                // amount of batch runs
    double ns_per_op_median;
    double ns_per_op_min;
    double ns_per_op_max;
    double total_ms;
};

void report(const BenchResult& br) {
    std::cout << "Type: " << br.benchmark_type << '\n';   
    std::cout << "\tIterations: " << br.iterations << '\n';   
    std::cout << "\tRepetitions: " << br.repetitions << '\n';   
    std::cout << "\tns Per Op Median: " << br.ns_per_op_median << '\n';   
    std::cout << "\tns Per Op Min: " << br.ns_per_op_min << '\n';   
    std::cout << "\tns Per Op Max: " << br.ns_per_op_max << '\n';   
    std::cout << "\tTotal ms: " << br.total_ms << '\n';   
}

// sink to ensure that our fn calls are not optimized away
template <class T> inline void keep(T& v) { asm volatile("" : "+r"(v) : : "memory"); }

/*
    std::function here would add in a few ns of overhead (2-5ns)
    tempalte allows us to study the callable itself without this overhead
    targeting around 100ms total runtime on these so need to tune iters/reps accordingly
*/
template <class F, class Setup>
BenchResult measure(const char* name, std::int64_t iters, int reps, F&& fn, Setup&& setup) {

    if (iters <= 0 || reps <= 0) { std::fprintf(stderr, "iter/reps must be > 0\n"); std::abort(); }
    
    int warmup_iters {5'000'000};
    std::int64_t res{};
    std::vector<double> ns_per_rep{};
    ns_per_rep.reserve(static_cast<std::size_t>(reps));

    setup();

    for (std::int64_t i = 0; i < warmup_iters; ++i) {
        auto v = fn();
        keep(v);
        res += v;
    }

    for (int i = 0; i < warmup_iters; ++i) {
        res += fn();
    }

    for (int r = 0; r < reps; r++) {
        setup();
        auto t0 = std::chrono::steady_clock::now();
        for (std::int64_t i = 0; i < iters; ++i) {
            auto v = fn();
            keep(v);          // forces the compiler not to optimize away the fn call or the iteration itself
            res += v;
        }
        auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        ns_per_rep.push_back(ns / static_cast<double>(iters));
        keep(res);
    }

    // used to get the median
    const auto mid = static_cast<std::ptrdiff_t>(ns_per_rep.size() / 2);
    std::nth_element(ns_per_rep.begin(), ns_per_rep.begin() + mid, ns_per_rep.end());

    const auto [min, max] = std::minmax_element(ns_per_rep.begin(), ns_per_rep.end());

    BenchResult br {
        .benchmark_type = name,
        .iterations = iters,
        .repetitions = reps,
        .ns_per_op_median = ns_per_rep[ns_per_rep.size() / 2],
        .ns_per_op_min = *min,
        .ns_per_op_max = *max,
        .total_ms = std::accumulate(ns_per_rep.begin(), ns_per_rep.end(), 0.0) * static_cast<double>(iters) / 1e6
    };

    return br;
}

OrderBook make_book(int n_levels, std::int64_t gap_ticks, Price best, std::int64_t base_qty) {
    std::vector<PriceLevel> bids, asks;
    // best bid 75,611 ticks ($7.5611), 130 levels, ~12 ticks apart, qty ~20,000 units
    for (int i = 0; i < n_levels; ++i) {
        bids.push_back({ Price{ best.ticks() - gap_ticks * i }, base_qty + 137 * i });
        asks.push_back({ Price{ best.ticks() + gap_ticks * i }, base_qty + 91  * i });
    }
    OrderBook bk;
    bk.apply_snapshot(Side::buy,  bids);
    bk.apply_snapshot(Side::sell, asks);

    if (bk.level_count(Side::buy) != static_cast<std::size_t>(n_levels)) std::abort();

    return bk;
}

int main() {
    constexpr std::int64_t iterations = 5'000'000;
    constexpr int repetitions = 7;

    report(measure("calib: empty loop", iterations, repetitions,
        [] { return std::int64_t{0}; },
        [] {}));

    OrderBook small_ob = make_book(130,  12, Price{75'611}, 20'000);
    OrderBook large_ob = make_book(2000, 12, Price{75'611}, 20'000);
    OrderBook mutable_ob = make_book(130, 12, Price{75'611}, 20'000);

    const Price present{ 75'611 - 12 * 60 };   // mid-book, definitely there
    const Price absent{ 75'611 - 12 * 60 - 3 }; // hangs off a real price, not in the book
    const Price update{75'611 - 12 * 60};

    const std::pair<const char*, OrderBook*> books[] = {
        {"small 130lv",  &small_ob},
        {"large 2000lv", &large_ob},
        {"mutable 130lv", &mutable_ob},
    };

    for (const auto& [label, ob] : books) {
        char name[96];

        auto run = [&](const char* what, auto&& fn) {
            std::snprintf(name, sizeof name, "%s / %s", label, what);
            report(measure(name, iterations, repetitions, fn, [] {}));
        };

        run("qty_at hit",   [ob] { return ob->qty_at(Side::buy, Price{75'611 - 12 * 60}); });
        run("qty_at miss",  [ob] { return ob->qty_at(Side::buy, Price{75'611 - 12 * 60 - 3}); });
        run("best_qty",     [ob] { return ob->best_qty(Side::buy); });
        run("level_count",  [ob] { return static_cast<std::int64_t>(ob->level_count(Side::buy)); });
        run("is_crossed",   [ob] { return static_cast<std::int64_t>(ob->is_crossed()); });
        run("depth 10",     [ob] { return static_cast<std::int64_t>(ob->depth_within(Side::buy, 10)); });
        run("depth 50",     [ob] { return static_cast<std::int64_t>(ob->depth_within(Side::buy, 50)); });
        run("depth 200",    [ob] { return static_cast<std::int64_t>(ob->depth_within(Side::buy, 200)); });
    }

    report(measure("apply_delta update", iterations, repetitions,
        [&mutable_ob, update] { return static_cast<std::int64_t>(mutable_ob.apply_delta(Side::buy, update, +1)); },
        [&mutable_ob] { mutable_ob = make_book(130, 12, Price{75'611}, 20'000); }));

    std::int64_t k = 0;
    report(measure("apply_delta create+erase (pair)", iterations, repetitions,
        [&mutable_ob, &k] {
            const Price p{ 60'000 + k };
            const auto r1 = static_cast<std::int64_t>(mutable_ob.apply_delta(Side::buy, p, 100));
            const auto r2 = static_cast<std::int64_t>(mutable_ob.apply_delta(Side::buy, p, -100));
            ++k;
            return r1 + r2;
        },
        [&mutable_ob, &k] { k = 0; mutable_ob = make_book(130, 12, Price{75'611}, 20'000); }));

    return 0;
}
