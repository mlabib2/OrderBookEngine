# Low-Latency Order Book Engine

A high-performance order book matching engine built in C++, with Redis pub/sub for real-time trade distribution and Python bindings for strategy development.

## Project Status

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Core C++ Engine | ✅ Complete |
| 2 | Redis Integration | ✅ Complete |
| 3 | Python Bindings + Live Market Data + Redis wiring | ✅ Complete |
| 4 | Docker + Backtesting + Strategy | ⬜ Planned |

See [PLAN.md](PLAN.md) for detailed progress tracking.

## What's Built

- **Price-time priority matching engine** — O(log n) add, O(1) cancel
- **Order types** — Limit and Market orders
- **Unit tests** — GoogleTest suite covering all core operations
- **Benchmarks** — Google Benchmark measuring real latency
- **Redis pub/sub** — C++ engine publishes trades to a Redis channel in real time
- **Python subscriber** — listens on Redis and prints trades as they happen
- **pybind11 bindings** — call the C++ engine directly from Python
- **Binance WebSocket feed** — streams live BTCUSDT order book data into the C++ engine every 100ms
- **End-to-end trade pipeline** — matched trades flow: Binance → Python → C++ engine → Redis → subscriber
- **GitHub Actions CI** — builds and tests on every push (GCC + Clang, Debug + Release)

## Benchmark Results

| Operation | Latency | Throughput |
|-----------|---------|------------|
| Add order | 0.14 µs | 7M orders/sec |
| Cancel order | 0.13 µs | 8M orders/sec |
| Match order | 0.22 µs | 4.5M orders/sec |
| Best bid/ask query | 2.6 ns | 390M queries/sec |

## Project Structure

```
OrderBookEngine/
├── PLAN.md                 # Master plan and progress tracker
├── docs/
│   ├── ARCHITECTURE.md     # Data structures and design rationale
│   └── GLOSSARY.md         # Trading terminology explained
├── cpp/
│   ├── CMakeLists.txt      # Build configuration
│   ├── include/            # Header files (.hpp)
│   ├── src/                # Implementation files (.cpp)
│   ├── tests/              # GoogleTest unit tests
│   └── benchmarks/         # Google Benchmark latency tests
└── python/                 # (Phase 3)
```

## Building

**Dependencies:** CMake 3.16+, C++17 compiler, hiredis

```bash
# macOS
brew install hiredis

# Ubuntu
apt install libhiredis-dev
```

```bash
cd cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/orderbook_demo
```

## Running the Redis Demo

Start Redis, then in one terminal subscribe to trades:
```bash
redis-cli SUBSCRIBE trades
```

In another terminal run the demo:
```bash
./cpp/build/orderbook_demo
```

You will see trades published by the C++ engine appear in the subscriber terminal in real time.

## Documentation

- [Architecture](docs/ARCHITECTURE.md) - Design decisions and data structures
- [Glossary](docs/GLOSSARY.md) - Trading terms explained
- [Plan](PLAN.md) - Full roadmap and progress tracker

## License

MIT
