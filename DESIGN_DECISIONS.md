# Design Decisions & Engineering Notes

---

## Architecture Overview

Three-layer pipeline:

```
Binance WebSocket (100ms)
        ↓
engine container — binance_feed.py calls C++ OrderBook via pybind11
        ↓ Redis PUBLISH
redis container — in-memory message broker
        ↓ Redis SUBSCRIBE
subscriber container — prints matched trades
```

**Three Docker containers:**
- `redis` — official Redis image, in-memory broker
- `engine` — builds C++ from source, runs `binance_feed.py` (calls C++ via pybind11)
- `subscriber` — lightweight Python image, runs `subscriber.py`

Containers communicate over Docker's internal network using the service name `redis` as hostname (`REDIS_HOST=redis`).

---

## Price Storage — `int64_t` over `double`

Computers store numbers in binary. Most decimals cannot be represented exactly — `0.1 + 0.2 = 0.30000000000000004`. In an order book, a wrong price comparison means a wrong match, which means money lost.

**Fix:** multiply price by 1,000,000 and store as `int64_t`.
- `$100.50 → 100,500,000`
- Integer comparisons are exact and faster than float operations.

**Tradeoff:** convert at system boundaries only.
- Incoming (Binance WebSocket) → `price_to_fixed()` immediately
- Outgoing (display/logging) → `price_to_double()` only

`double` appears in exactly two utility functions — nowhere inside the engine itself.

---

## Data Structure — `std::map` over `std::unordered_map`

| | `std::map` | `std::unordered_map` |
|---|---|---|
| Underlying structure | Red-black tree | Hash table |
| Insert | O(log n) | O(1) average |
| Find min/max | O(1) via `begin()` / `rbegin()` | O(n) — unordered |

An order book constantly needs the best bid (highest price) and best ask (lowest price). With `std::map`, that's `rbegin()` and `begin()` — O(1). With a hash map, you'd scan all keys — O(n).

The sorted order is worth the O(log n) insertion cost.

- **Bids** — descending order. Highest bid first — incoming sell matches against the buyer willing to pay the most.
- **Asks** — ascending order. Lowest ask first — incoming buy matches against the seller asking the least.

This is **price-time priority**: best price first, earliest order first at the same price level.

---

## Redis Pub/Sub — over Direct Calls or a Database

**Why not a direct function call?**
A direct function call only works within the same process. The C++ engine and Python subscriber are separate processes — they cannot call each other directly. pybind11 works within the same process (Binance feed calling C++ engine), but the subscriber is fully decoupled.

**Why not a shared database?**
Databases write to disk — milliseconds latency. Redis operates entirely in memory — microseconds. Pub/sub is designed for real-time event broadcasting, not persistence. Trades don't need to be stored permanently — they need to be delivered instantly.

**Benefit of decoupling:** any number of subscribers in any language can listen to the `trades` channel simultaneously. The engine doesn't know or care who is listening.

**If Redis goes down:** the engine fails to publish trades — they are silently lost. The matching engine itself keeps running. The subscriber disconnects immediately.

---

## Error Codes over Exceptions

Throwing a C++ exception takes **1,000+ nanoseconds** — the runtime must unwind the call stack and allocate heap memory for the exception object.

Our cancel latency target was under 1,000ns. One exception would blow the entire budget.

Error codes are integer return values — a single comparison. Essentially free (~1ns).

**Downside:** error codes are easy to ignore. Nothing forces the caller to check them — errors can silently propagate. Exceptions crash the program if unhandled; error codes do not.

---

## Cache Line Optimization — `Order` Struct Layout

A CPU cache line is **64 bytes** fetched at once from RAM. When the CPU loads one field, it automatically pulls in the surrounding 64 bytes.

Hot fields (accessed every match) are declared first in the `Order` struct:
- `id` (8) + `price` (8) + `quantity` (8) + `filled_quantity` (8) + `side` (1) + `type` (1) + `status` (1) = 35 bytes — fits in one cache line.

Cold fields (`timestamp`, `symbol`) are declared last — rarely touched during matching.

No special compiler directives needed. Field ordering in the struct is the entire implementation. The CPU manages promotion between L1 (~1ns), L2 (~5ns), and L3 (~20ns) automatically based on access frequency.

---

## Debugging — Redis Host in Docker

**Problem:** Python feed was hardcoded to `127.0.0.1` (localhost). This worked locally but failed in Docker.

**Why it broke:** `127.0.0.1` means "this machine right here." In Docker, each container is isolated — `127.0.0.1` inside the engine container points to itself, not the Redis container. Redis is in a separate container.

**How Docker networking works:** Docker Compose creates a shared internal network. Each container is reachable by its service name. Redis is reachable at hostname `redis` from any other container.

**Fix:** Read the host from an environment variable with a sensible default:
```python
redis_host = os.environ.get("REDIS_HOST", "127.0.0.1")
```
- Locally — no env var set, defaults to `127.0.0.1`. Works.
- In Docker — `docker-compose.yml` sets `REDIS_HOST=redis`. Works.

Same code, two environments, no hardcoding.

---

## Docker — Why Engine and Binance Feed Share a Container

`binance_feed.py` calls the C++ engine via pybind11 — it imports the compiled `.so` file directly from the same filesystem. They are tightly coupled. Separating them would mean copying the `.so` across containers, adding complexity for no benefit.

The subscriber has no dependency on C++ at all — it only reads from Redis. It naturally belongs in its own lightweight container.

Could we separate them? Yes — if we had multiple feed sources and needed independent scaling. For this scope, keeping them together is simpler.

---

## Price-Time Priority

Matching follows two rules in order:

**1. Price priority**
- Bids sorted descending (`std::greater<Price>`) — highest bid matched first
- Asks sorted ascending (`std::less<Price>`) — lowest ask matched first
- `asks_.begin()` and `bids_.begin()` always return the best price in O(1)

**2. Time priority**
- Within a price level, orders sit in a FIFO queue
- `level.front()` always returns the earliest arriving order
- If two orders are at the same price, the one that arrived first is filled first

**Example:** Buy 70 BTC at $100. Ask side:
```
$100 → [Order A: 50 BTC, arrived 1st] → [Order B: 30 BTC, arrived 2nd]
$101 → [Order C: 100 BTC]
```
- Price priority → match at $100 (lowest ask)
- Time priority → fill Order A (50 BTC) fully first, then take 20 BTC from Order B
- Result: Order A gone, Order B has 10 BTC remaining on book

**In the code:**
- Line 83 `opposite_book.begin()` — best price first
- Line 92 `level.front()` — earliest order first

---

## Latency vs Throughput

- **Throughput** — number of orders processed per second
- **Latency** — time to process one order (e.g. time from `add_order()` being called to returning)

They are inversely related: `Throughput ≈ 1 / Latency`

```
1 / 0.22µs = ~4.5M orders/sec
```

In this system they are **consistent, not in conflict** — orders are processed independently with no queuing, locking, or shared state between operations.

In more complex systems they trade off:
- **Batching** — group 1000 orders together → higher throughput, higher per-order latency
- **Mutex locks** — protect shared state → correct but serializes access, reducing throughput
- **Network buffering** — accumulate packets → higher throughput, higher latency

---

## Production Gaps — What Would Be Added

**1. Persistence — replace Redis pub/sub with Kafka or Redis Streams**
Redis pub/sub is fire-and-forget. If the subscriber is offline for 1 second, those trades are permanently lost. Kafka or Redis Streams persist messages to disk so they can be replayed or audited. Every trade must be auditable for regulatory compliance. Kafka is battle-tested at scale in financial systems.

**2. Monitoring and alerting — Prometheus + Grafana**
If the Binance WebSocket disconnects, the engine silently stops processing. In production, metrics must be tracked — order throughput, latency percentiles, WebSocket reconnection attempts. If throughput drops to zero, an alert fires immediately.

**3. Fault tolerance — automatic reconnection and restart policies**
If the WebSocket drops, the feed dies. Production systems need automatic reconnection with exponential backoff, container health checks, and Docker restart policies so components recover without manual intervention.

---

## pybind11 — Bridging C++ and Python

Python cannot call C++ directly because they are different languages with different runtimes and memory models. C++ is compiled to native machine code; Python is interpreted. Python has no way to understand C++ types, function signatures, or memory layout natively.

pybind11 acts as an intermediary:
- Wraps C++ functions and classes with Python-compatible interfaces
- Handles type conversion both ways (`int64_t` ↔ Python `int`, `double` ↔ Python `float`)
- Manages memory safely across the boundary (Python GC vs C++ manual memory)
- Compiles everything into a `.so` shared library (Linux/Mac) that Python imports like any regular module

```python
import orderbook_core          # importing the compiled .so
book = orderbook_core.OrderBook()   # calling C++ from Python
```

Python has no idea it's calling C++ — it looks like a normal Python module.

This is different from Redis pub/sub — pybind11 works within the same process (Binance feed + C++ engine). Redis works across separate processes (engine → subscriber).

---

## Market-Making Strategy (`python/strategy.py`)

A market maker provides liquidity by continuously posting both buy and sell quotes, profiting from the bid-ask spread rather than price direction.

**Parameters:**
- `MAX_POSITION = 5.0 BTC` — inventory risk cap
- `MIN_CASH = $1,000` — always keep a cash reserve
- `HALF_SPREAD = 0.1%` — post quotes 0.1% away from mid on each side

**Decision logic per tick:**
```
1. If BTC held ≥ 5       → force SELL  (inventory cap)
2. If cash ≤ $1,000      → HOLD        (cash reserve)
3. mid = (best_bid + best_ask) / 2
   our_bid = mid × 0.999
   our_ask = mid × 1.001
4. If market ask ≤ our_ask → BUY   (lift the offer — someone selling cheap)
5. If market bid ≥ our_bid → SELL  (hit the bid — someone buying high)
6. Otherwise               → HOLD
```

**Profit mechanism:** buy at bid, sell at ask, collect the 0.2% spread. Repeat at high frequency.

**Why it underperformed in backtest:** daily bars are too coarse — real market making operates at microsecond tick granularity. At daily resolution the strategy just tracks price direction, not the spread.

---

## Sharpe Ratio

**Formula:** (Return − Risk-free rate) / Standard deviation of returns

The risk-free rate is the return on government bonds — what you'd earn with zero risk.

| Sharpe | Interpretation |
|--------|---------------|
| > 2 | Excellent |
| > 1 | Good |
| 0 to 1 | Marginal |
| < 0 | Losing money per unit of risk — worse than government bonds |

**-0.52 in this system:** for every unit of risk taken, we lose 0.52 units of return. Significant volatility with negative returns. Confirms the strategy isn't suited to daily data — the granularity is the problem, not the logic.

---

## Backtesting Result — Interpreting -30.9%

**Result:** Total return -30.9% | Sharpe -0.52 | Max drawdown 49.4%

This is expected and does not mean the strategy is broken.

Market making profits from capturing the bid-ask spread thousands of times per second — it requires tick-level microsecond data. The backtest used daily OHLCV bars from yfinance. At that granularity, the strategy is effectively just tracking price direction. BTC declined ~30% over the period, so the strategy followed it down.

- **Sharpe -0.52** — negative risk-adjusted returns
- **Max drawdown 49.4%** — portfolio lost nearly half at its worst point

**Why not use tick data?** Tick data at microsecond granularity costs money (Tardis.dev, Kaiko). For a project demonstrating architecture, free daily data validates the pipeline end-to-end. With real tick data the same code runs unchanged.

**Would it work with tick data?** Possibly — but real crypto market making competes against HFT firms with co-located servers. Profitability depends on execution quality, fees, and adverse selection — factors this backtest doesn't model.

---

## CI Pipeline — GitHub Actions

- **Matrix:** Ubuntu × [GCC, Clang] × [Debug, Release]
- **Steps:** build → run unit tests (GoogleTest) → run sanitizers

**Why both GCC and Clang?**
Different compilers catch different warnings. Code that compiles cleanly on one may have warnings or subtle bugs on the other. Running both ensures portable, standard-compliant C++.

**What is a sanitizer?**
Compiler instrumentation that detects bugs at runtime:
- **ASan (AddressSanitizer)** — detects memory errors: buffer overflows, use-after-free, heap corruption
- **UBSan (UndefinedBehaviorSanitizer)** — detects undefined behavior: integer overflow, null pointer dereference

Sanitizers only run on Debug builds because they add runtime overhead (~2x slower). They are critical for C++ in trading systems where memory bugs cause silent data corruption.
