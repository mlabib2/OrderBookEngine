# Low-Latency Order Book Engine - Master Plan

## Project Philosophy

- **KISS**: Keep It Simple, Stupid - start with the simplest working version
- **DRY**: Don't Repeat Yourself - extract common patterns into reusable components
- **Incremental**: Build one small piece at a time, test it, understand it
- **Learn**: Understand WHY each design choice is made

---

## Progress Tracker

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Core C++ Engine + Tests + CI | ✅ Complete |
| 2 | Redis Integration | ✅ Complete |
| 3 | Python Bindings + Live Market Data | ✅ Complete |
| 4 | Docker + Backtesting + Strategy | ✅ Complete |

---

## Phase 1: Core Order Book

| Step | Description | Status |
|------|-------------|--------|
| 0 | Project Setup & Documentation | ✅ Complete |
| 1 | Types Foundation (`types.hpp`) | ✅ Complete |
| 2 | Order Structure (`order.hpp`) | ✅ Complete |
| 3 | Trade Structure (`trade.hpp`) | ✅ Complete |
| 4 | PriceLevel Class | ✅ Complete |
| 5 | OrderBook - Data Only | ✅ Complete |
| 6 | OrderBook - Matching | ✅ Complete |
| 7 | Unit Tests | ✅ Complete |
| 8 | Benchmarks | ✅ Complete |
| 9 | GitHub Actions CI | ✅ Complete |

### What was built
- `cpp/include/types.hpp` — fixed-point price type (`int64_t`, 6 decimals), OrderId, Quantity
- `cpp/include/order.hpp` — Order struct (id, side, price, quantity, timestamp)
- `cpp/include/trade.hpp` — Trade struct (buyer/seller order ids, price, quantity)
- `cpp/include/price_level.hpp` + `cpp/src/price_level.cpp` — FIFO queue at a single price point
- `cpp/include/order_book.hpp` + `cpp/src/order_book.cpp` — price-time priority matching engine
- `cpp/tests/` — GoogleTest suite covering add, cancel, match, edge cases
- `cpp/benchmarks/latency_benchmark.cpp` — Google Benchmark: add (0.14µs), cancel (0.13µs), match (0.22µs), query (2.6ns)
- `.github/workflows/ci.yml` — Ubuntu × [GCC, Clang] × [Debug, Release], runs tests + sanitizers

### CI Pipeline
```
Trigger: push + pull_request
│
├── Build Matrix: Ubuntu × [GCC, Clang] × [Debug, Release]
│
├── Steps:
│   ├── Build project (CMake + FetchContent)
│   ├── Run unit tests (GoogleTest)
│   └── Run sanitizers (ASan + UBSan on Debug)
│
└── Artifacts: Test results per compiler/config
```

---

## Phase 2: Redis Integration

| Step | Description | Status |
|------|-------------|--------|
| 10 | Understand Redis pub/sub | ✅ Complete |
| 11 | C++ publishes trades via hiredis | ✅ Complete |
| 12 | Python subscriber prints trades | ✅ Complete |

### What was built
- `cpp/include/redis_publisher.hpp` + `cpp/src/redis_publisher.cpp` — `RedisPublisher` wraps hiredis, connects on construction, publishes trade events to the `trades` channel
- `cpp/src/main.cpp` — demo wiring OrderBook + RedisPublisher: adds matching orders, trade published to Redis
- `python/subscriber.py` — subscribes to `trades` channel, prints each trade received
- hiredis built from source via FetchContent (no system package required, works on all platforms)

---

## Phase 3: Python Bindings + Live Market Data

| Step | Description | Status |
|------|-------------|--------|
| 13 | pybind11 bindings — call C++ engine from Python | ✅ Complete |
| 14 | Binance WebSocket — stream live BTCUSDT data | ✅ Complete |
| 15 | Wire together — live data feeds C++ engine | ✅ Complete |
| 16 | Publish matched trades from Python to Redis | ✅ Complete |

### What was built
- `cpp/bindings/orderbook_bindings.cpp` — pybind11 module exposing `OrderBook` and `Trade` to Python
- `python/binance_feed.py` — connects to Binance WebSocket (`btcusdt@depth10@100ms`), feeds best bid/ask into C++ engine every 100ms, publishes matched trades to Redis
- `python/subscriber.py` — subscribes to Redis `trades` channel and prints each trade
- `python/requirements.txt` — redis, pybind11, websocket-client

### Full pipeline (verified live)
```
Binance WebSocket (100ms)
        ↓
binance_feed.py (Python)
        ↓ pybind11
C++ OrderBook.add_order()  →  returns matched Trade objects
        ↓ redis-py
Redis PUBLISH "trades"
        ↓
subscriber.py  →  prints trade
```

---

## Phase 4: Docker + Backtesting + Strategy

| Step | Description | Status |
|------|-------------|--------|
| 17 | Docker Compose — containerize full pipeline | ✅ Complete |
| 18 | Backtesting framework — replay historical data | ✅ Complete |
| 19 | Market-making strategy | ✅ Complete |

### Step 17: Docker Compose ✅
**Files**: `Dockerfile.engine`, `Dockerfile.subscriber`, `docker-compose.yml`, `.dockerignore`

Three containers:
- `redis` — official `redis:7-alpine` image, healthcheck ensures it's ready before dependents start
- `engine` — Ubuntu 22.04, builds C++ engine from source, runs `binance_feed.py`
- `subscriber` — Python slim image, runs `subscriber.py`

`REDIS_HOST` environment variable lets the same Python code work locally (`127.0.0.1`) and in Docker (`redis` service name).

### Step 18: Backtesting ✅
**File**: `python/backtest.py`
**Goal**: Replay 1 year of historical BTCUSDT daily data through the C++ engine tick by tick
**Key metrics**: Total return, Sharpe ratio, max drawdown, days traded
**How it works**: yfinance downloads OHLCV data → each day feeds a bid/ask into the C++ OrderBook via pybind11 → strategy decides buy/sell/hold → P&L tracked mark-to-market

### Step 19: Market-Making Strategy ✅
**File**: `python/strategy.py`
**Goal**: Strategy that uses order book state (best_bid, best_ask, mid price) — not moving averages
**Logic**:
- Compute mid = (best_bid + best_ask) / 2
- Post quotes HALF_SPREAD (0.1%) away from mid on each side
- Buy when market ask ≤ our target ask (lift the offer)
- Sell when market bid ≥ our target bid (hit the bid)
- Inventory cap: never hold more than 5 BTC (risk control)
- Cash reserve: always keep $1,000 minimum

**Backtest result** (1 year BTC-USD daily data, $100K starting capital):
- Total return: -30.9% | Sharpe: -0.52 | Max drawdown: 49.4%
- Result is expected: BTC declined over the period; daily bars are too coarse for real market making (which operates at microsecond granularity with tick data)

---

## Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Price type | `int64_t` (6 decimals) | $100.50 = 100500000. Avoids float comparison bugs. |
| Error handling | Error codes | Faster than exceptions for hot paths |
| hiredis | FetchContent (built from source) | No system package required, works on macOS + Ubuntu CI |
| pybind11 | FetchContent | Same reason; also pins exact version |
| orderbook_core | POSITION_INDEPENDENT_CODE ON | Required for pybind11 .so on Linux |
| Python host | Homebrew Python 3.12 | System 3.9 has no dev headers; conda .app bundle breaks pybind11 |
| Redis host | Env var (REDIS_HOST) | Same code works locally and inside Docker |
| CI compilers | GCC + Clang | Catches different warnings, ensures portable code |
| CI sanitizers | ASan + UBSan | Catches memory bugs, critical for C++ in trading |

---

## Performance Results

| Operation | Latency | Throughput |
|-----------|---------|------------|
| Add order | 0.14 µs | 7M orders/sec |
| Cancel order | 0.13 µs | 8M orders/sec |
| Match order | 0.22 µs | 4.5M orders/sec |
| Best bid/ask query | 2.6 ns | 390M queries/sec |

## Performance Targets

| Metric | Target | Status |
|--------|--------|--------|
| Order add latency | < 10 µs | ✅ 0.14 µs |
| Order cancel latency | < 1 µs | ✅ 0.13 µs |
| Book query latency | < 1 µs | ✅ 2.6 ns |
| Throughput | > 100,000 orders/sec | ✅ 7M/sec |
