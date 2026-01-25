# Low-Latency Order Book Engine

A high-performance order book matching engine built in C++ with Python bindings for strategy development and backtesting.

## Project Status

**Phase 1: Core Order Book** - In Progress

See [PLAN.md](PLAN.md) for detailed progress tracking.

## Features (Planned)

- **C++ Matching Engine**: Price-time priority matching with <10µs latency
- **Order Types**: Limit and Market orders
- **Python Bindings**: Use the engine from Python via pybind11
- **Backtesting**: Test strategies on historical data
- **Redis Integration**: Real-time state sharing and pub/sub

## Project Structure

```
OrderBookEngine/
├── PLAN.md                 # Master plan and progress tracker
├── docs/
│   ├── ARCHITECTURE.md     # Data structures and design rationale
│   └── GLOSSARY.md         # Trading terminology explained
├── cpp/
│   ├── CMakeLists.txt      # Build configuration
│   ├── include/            # Header files
│   ├── src/                # Implementation files
│   ├── tests/              # Unit tests
│   └── benchmarks/         # Performance benchmarks
└── python/                 # (Phase 3+)
```

## Building (Coming Soon)

```bash
cd cpp
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

## Performance Targets

| Metric | Target |
|--------|--------|
| Order add latency | < 10 µs |
| Order cancel latency | < 1 µs |
| Throughput | > 100,000 orders/sec |

## Documentation

- [Architecture](docs/ARCHITECTURE.md) - Why we made each design decision
- [Glossary](docs/GLOSSARY.md) - Trading terms explained

## License

MIT
