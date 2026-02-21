# Low-Latency Order Book Engine - Master Plan

## Project Philosophy

- **KISS**: Keep It Simple, Stupid - start with the simplest working version
- **DRY**: Don't Repeat Yourself - extract common patterns into reusable components
- **Incremental**: Build one small piece at a time, test it, understand it
- **Learn**: Understand WHY each design choice is made

---

## Progress Tracker

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

---

## Phase 1: Core Order Book (Current Focus)

### Step 0: Project Setup & Documentation
Create the foundation:
```
OrderBookEngine/
├── PLAN.md                    # This file
├── docs/
│   ├── ARCHITECTURE.md        # System design & data structures
│   └── GLOSSARY.md            # Trading terms explained
├── cpp/
│   ├── CMakeLists.txt
│   ├── include/
│   ├── src/
│   ├── tests/
│   └── benchmarks/
└── .gitignore
```

### Step 1: Types Foundation
**File**: `cpp/include/types.hpp`
**Goal**: Define core types that everything else builds on
**Learn**: Fixed-point arithmetic, why we avoid floats for money

### Step 2: Order Structure
**File**: `cpp/include/order.hpp`
**Goal**: Simple Order struct with essential fields only
**Learn**: Memory layout, struct design

### Step 3: Trade Structure
**File**: `cpp/include/trade.hpp`
**Goal**: Trade struct representing a completed match
**Learn**: Separation of concerns (Order vs Trade)

### Step 4: PriceLevel Class
**Files**: `cpp/include/price_level.hpp`, `cpp/src/price_level.cpp`
**Goal**: Manage orders at a single price point (FIFO queue)
**Learn**: std::list, why FIFO matters for fairness

### Step 5: OrderBook - Data Only
**Files**: `cpp/include/orderbook.hpp`, `cpp/src/orderbook.cpp`
**Goal**: Store bids/asks, add to book, query best prices (NO matching yet)
**Learn**: std::map with custom comparators, O(1) lookups

### Step 6: OrderBook - Matching
**Goal**: Add matching logic to OrderBook
**Learn**: Price-time priority algorithm, trade generation

### Step 7: Unit Tests
**Files**: `cpp/tests/test_*.cpp`
**Goal**: Verify everything works correctly
**Learn**: GoogleTest basics

### Step 8: Benchmarks
**File**: `cpp/benchmarks/latency_benchmark.cpp`
**Goal**: Measure performance against targets (<10µs add, <1µs cancel)
**Learn**: Google Benchmark

### Step 9: GitHub Actions CI
**File**: `.github/workflows/ci.yml`
**Goal**: Automated build, test, and benchmark tracking on every push
**Learn**: CI/CD pipelines, continuous performance monitoring

**Pipeline:**
```
Trigger: push + pull_request
│
├── Build Matrix: Ubuntu × [GCC, Clang] × [Debug, Release]
│
├── Steps:
│   ├── Build project (CMake)
│   ├── Run unit tests (GoogleTest)
│   ├── Run sanitizers (ASan + UBSan on Debug)
│   └── Run benchmarks + track regression ⭐
│
└── Artifacts: Benchmark results, status badge
```

**Why this matters:**
- Multi-compiler catches portability issues
- Sanitizers catch memory bugs (critical for C++)
- Benchmark tracking shows performance awareness (impresses recruiters)

---

## Phase 2: Redis Integration

| Step | Description | Status |
|------|-------------|--------|
| 10 | Install Redis + understand pub/sub | ✅ Complete |
| 11 | C++ publishes trades via hiredis | ✅ Complete |
| 12 | Python subscriber prints trades | 🔄 In Progress |

### What was built
- `cpp/include/redis_publisher.hpp` + `cpp/src/redis_publisher.cpp` — `RedisPublisher` class wraps hiredis, connects on construction, publishes trade events to the `trades` channel
- `cpp/src/main.cpp` — demo wiring OrderBook + RedisPublisher together

---

## Future Phases (Overview)

| Phase | Focus | Key Components |
|-------|-------|----------------|
| 3 | Market Data + Python | Binance WebSocket, pybind11 |
| 4 | Backtesting + Polish | Python engine, strategies, Docker |

---

## Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Price type | `int64_t` (6 decimals) | $100.50 = 100500000. Avoids float comparison bugs. |
| Error handling | Error codes | Faster than exceptions for hot paths |
| Order ownership | MatchingEngine owns | OrderBook holds pointers, simpler memory management |
| CI compilers | GCC + Clang | Catches different warnings, ensures portable code |
| CI sanitizers | ASan + UBSan | Catches memory bugs, critical for C++ in trading |
| Benchmark tracking | Yes | Prevents perf regressions, shows professionalism |
| Phase 1 scope | C++ engine + tests + CI | No Redis, Python, or WebSocket yet |

---

## Performance Targets

| Metric | Target |
|--------|--------|
| Order add latency | < 10 µs |
| Order cancel latency | < 1 µs |
| Book query latency | < 1 µs |
| Throughput | > 100,000 orders/sec |
| Memory per order | < 200 bytes |
