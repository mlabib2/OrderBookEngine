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
| 4 | PriceLevel Class | Not Started |
| 5 | OrderBook - Data Only | Not Started |
| 6 | OrderBook - Matching | Not Started |
| 7 | Unit Tests | Not Started |
| 8 | Benchmarks | Not Started |

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

---

## Future Phases (Overview)

| Phase | Focus | Key Components |
|-------|-------|----------------|
| 2 | Redis Integration | State sharing, pub/sub |
| 3 | Market Data + Python | Binance WebSocket, pybind11 |
| 4 | Backtesting + Polish | Python engine, strategies, Docker |

---

## Key Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Price type | `int64_t` (6 decimals) | $100.50 = 100500000. Avoids float comparison bugs. |
| Error handling | Error codes | Faster than exceptions for hot paths |
| Order ownership | MatchingEngine owns | OrderBook holds pointers, simpler memory management |
| Phase 1 scope | C++ engine + tests only | No Redis, Python, or WebSocket yet |

---

## Performance Targets

| Metric | Target |
|--------|--------|
| Order add latency | < 10 µs |
| Order cancel latency | < 1 µs |
| Book query latency | < 1 µs |
| Throughput | > 100,000 orders/sec |
| Memory per order | < 200 bytes |
