#!/usr/bin/env python3
"""Extract a book-shape profile from kalshi-recorder oracle JSONL.

Usage:
    python3 tools/profile_books.py <oracle.jsonl> [--out path] [--top N]

Reads every checkpoint line, decodes the nested `raw` JSON, and pools all
observations per market. Prices/quantities are converted with Decimal so the
tick/unit values are exact (no float rounding) and therefore directly
comparable against MAX_PRICE_TICKS / MAX_QTY_UNITS.
"""
import argparse
import json
import os
import statistics
from decimal import Decimal

PRICE_SCALE = 10_000
QUANTITY_SCALE = 100


def ticks(s: str) -> int:
    return int((Decimal(s) * PRICE_SCALE).to_integral_value())


def units(s: str) -> int:
    return int((Decimal(s) * QUANTITY_SCALE).to_integral_value())


def pct(xs, p):
    if not xs:
        return 0
    xs = sorted(xs)
    k = min(len(xs) - 1, int(round((p / 100.0) * (len(xs) - 1))))
    return xs[k]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path")
    ap.add_argument("--out", default="benchmarks/fixtures/book_profile.json")
    ap.add_argument("--top", type=int, default=12)
    a = ap.parse_args()

    per_market = {}
    wall_ts = []
    lines = skipped = 0

    with open(a.path) as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            lines += 1
            try:
                rec = json.loads(line)
                payload = json.loads(rec["raw"])
                books = payload["orderbooks"]
            except (KeyError, ValueError, TypeError):
                skipped += 1
                continue
            if rec.get("wall_ts"):
                wall_ts.append(rec["wall_ts"])

            for entry in books:
                t = entry.get("ticker")
                ob = entry.get("orderbook") or {}
                bids = [(ticks(p), units(q)) for p, q in ob.get("bids", [])]
                asks = [(ticks(p), units(q)) for p, q in ob.get("asks", [])]
                m = per_market.setdefault(t, {"levels": [], "gaps": [], "qty": [],
                                              "prices": [], "snaps": 0})
                m["snaps"] += 1
                m["levels"].append((len(bids), len(asks)))
                for p, q in bids + asks:
                    m["prices"].append(p)
                    m["qty"].append(q)
                allp = sorted({p for p, _ in bids} | {p for p, _ in asks})
                m["gaps"].extend(b - a_ for a_, b in zip(allp, allp[1:]))

    rows = []
    global_max_price = global_max_qty = 0
    for t, m in sorted(per_market.items(), key=lambda kv: -statistics.median(
            [b for b, _ in kv[1]["levels"]] or [0]))[: a.top]:
        bid_levels = [b for b, _ in m["levels"]]
        ask_levels = [b for _, b in m["levels"]]
        gaps, qty, prices = m["gaps"], m["qty"], m["prices"]
        if not qty:
            continue
        global_max_price = max(global_max_price, max(prices))
        global_max_qty = max(global_max_qty, max(qty))
        rows.append({
            "ticker": t,
            "snapshots": m["snaps"],
            "levels_per_side_median": [int(statistics.median(bid_levels)),
                                       int(statistics.median(ask_levels))],
            "levels_max": [max(bid_levels), max(ask_levels)],
            "tick_gap": {"median": pct(gaps, 50), "p90": pct(gaps, 90), "max": max(gaps) if gaps else 0},
            "qty_units": {"min": min(qty), "median": pct(qty, 50),
                          "p90": pct(qty, 90), "max": max(qty)},
        })

    profile = {
        "source": {
            "file": a.path,
            "checkpoints": lines,
            "skipped_lines": skipped,
            "window": [min(wall_ts), max(wall_ts)] if wall_ts else None,
            "note": "weekend snapshot (equity/index markets halted); thin vs weekday liquidity",
        },
        "scales": {"price_ticks_per_unit": PRICE_SCALE, "qty_units_per_contract": QUANTITY_SCALE},
        "observed_maxima": {
            "price_ticks": global_max_price,
            "price_dollars": round(global_max_price / PRICE_SCALE, 4),
            "qty_units": global_max_qty,
            "qty_contracts": round(global_max_qty / QUANTITY_SCALE, 2),
        },
        "markets": rows,
    }

    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    with open(a.out, "w") as fh:
        json.dump(profile, fh, indent=2)
        fh.write("\n")

    print(f"{'ticker':16} {'snaps':>5} {'lv bid/ask':>12} {'gap med':>8} {'gap max':>8} "
          f"{'qty med':>12} {'qty max':>14}")
    for r in rows:
        print(f"{r['ticker']:16} {r['snapshots']:5} "
              f"{r['levels_per_side_median'][0]:5}/{r['levels_per_side_median'][1]:<6} "
              f"{r['tick_gap']['median']:8} {r['tick_gap']['max']:8} "
              f"{r['qty_units']['median']:12} {r['qty_units']['max']:14}")
    print(
        f"\nobserved maxima: price={global_max_price} ticks "
        f"(${global_max_price / PRICE_SCALE:.4f}), qty={global_max_qty} units "
        f"({global_max_qty / QUANTITY_SCALE:.2f} contracts)\nwrote {a.out}")


if __name__ == "__main__":
    main()
