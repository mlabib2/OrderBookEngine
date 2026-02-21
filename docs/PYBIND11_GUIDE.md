# pybind11 & Redis — What Was Built and Why

A plain-English walkthrough of Phases 2 and 3 for interview prep.

---

## Part 1: Redis Pub/Sub

### What is Redis?

Redis is a program that runs in the background on your machine and stores data in memory. Think of it like a Python dictionary that any program can read from or write to — across processes.

```
Program A  →  Redis  →  Program B
(C++)                    (Python)
```

### What is pub/sub?

Pub/sub = publish/subscribe. It's a messaging pattern:
- **Publisher**: sends a message to a named channel
- **Subscriber**: listens on that channel and receives every message

```
redis-cli SUBSCRIBE trades     ← subscriber (listening)
redis-cli PUBLISH trades "..."  ← publisher (sending)
```

Any number of subscribers can listen to the same channel. The publisher doesn't know or care who is listening.

### What did we build?

**`cpp/include/redis_publisher.hpp` + `cpp/src/redis_publisher.cpp`**

A C++ class with one job: when a trade happens in the engine, publish it to Redis.

```cpp
RedisPublisher publisher;           // connects to Redis on port 6379
publisher.publish_trade(trade);     // sends: "symbol=AAPL price=101.0 qty=100 ..."
```

Under the hood it uses `hiredis` — the official C client for Redis. One function call:
```cpp
redisCommand(ctx_, "PUBLISH trades %s", message.c_str());
```

**`python/subscriber.py`**

A Python script that listens and prints:
```python
pubsub.subscribe("trades")
for message in pubsub.listen():
    print(message['data'])   # prints whatever C++ published
```

### The full flow

```
1. book.add_order(&buy)         ← C++ engine matches buy vs sell
2. trades = [Trade{...}]        ← engine returns a trade
3. publisher.publish_trade(t)   ← C++ sends to Redis
4. Redis delivers to subscribers
5. Python prints the trade      ← subscriber.py receives and prints
```

---

## Part 2: pybind11

### What problem does it solve?

Python is slow. C++ is fast. Your matching engine needs to be fast — that's why it's in C++.

But connecting to Binance WebSocket, running strategy logic, and scripting is much easier in Python.

pybind11 lets you write C++ code and call it from Python as if it were a normal Python library:

```python
import orderbook_engine                        # this is your compiled C++ code
book = orderbook_engine.OrderBook("AAPL")
trades = book.add_order("buy", 101.0, 100)
print(trades[0].price())                       # → 101.0
```

### How does it work?

pybind11 compiles your C++ code into a `.so` file (shared library). Python can import `.so` files just like `.py` files.

```
orderbook_bindings.cpp  →  cmake  →  orderbook_engine.cpython-312-darwin.so
                                              │
                                    import orderbook_engine  (Python)
```

### The bindings file explained

**`cpp/bindings/orderbook_bindings.cpp`** — this is the glue code.

```cpp
PYBIND11_MODULE(orderbook_engine, m) {
```
This defines the Python module name. `import orderbook_engine` in Python maps to this.

```cpp
py::class_<Trade>(m, "Trade")
    .def_readonly("id", &Trade::id)
    .def_readonly("quantity", &Trade::quantity)
    .def("price", [](const Trade& t) { return price_to_double(t.price); })
```
This says: "expose the C++ `Trade` struct to Python as a class called `Trade`, with these readable fields".

```cpp
py::class_<OrderBook>(m, "OrderBook")
    .def(py::init<const std::string&>(), py::arg("symbol"))
    .def("add_order", [](OrderBook& book, const std::string& side,
                          double price, uint64_t quantity) {
        // build C++ Order from Python values, call real add_order
    })
```
This is the important one. Python has no concept of C++ pointers, so instead of exposing `add_order(Order*)` directly, we wrap it in a lambda that takes plain Python types (string, float, int) and builds the `Order` object internally.

### Why can't you just expose `add_order(Order*)` directly?

Because Python doesn't have pointers. Python can't create a `Order*`. So we hide that complexity inside the binding — Python passes simple values, C++ handles the pointer internally.

```python
# What Python sees (simple):
book.add_order("buy", 101.0, 100)

# What happens in C++ internally (hidden from Python):
Order* o = new Order(id++, symbol, Side::Buy, OrderType::Limit, 100, price_to_fixed(101.0));
book.add_order(o);
```

### Testing it

```python
import orderbook_engine

book = orderbook_engine.OrderBook("AAPL")

# Add a resting sell at $101
book.add_order("sell", 101.0, 100)

# Add an aggressive buy at $102 — this crosses and matches
trades = book.add_order("buy", 102.0, 100)

print(trades)            # → [AAPL qty=100 @ $101.000000]
print(book.best_bid())   # → None (book is empty, both orders filled)
print(book.best_ask())   # → None
```

---

## Interview Questions You Should Be Able to Answer

**Q: Why use C++ for the matching engine instead of Python?**
A: Python is 100x slower for this kind of tight loop work. The matching engine needs sub-microsecond latency. C++ with -O3 gives us 0.14µs per order. Python would be 10-50µs.

**Q: What does pybind11 actually do?**
A: It compiles C++ code into a shared library (.so file) that Python can import. It handles type conversion between Python and C++ automatically.

**Q: Why can't you expose `add_order(Order*)` directly to Python?**
A: Python has no concept of raw pointers. We wrap it in a lambda that accepts plain Python types and constructs the Order object in C++.

**Q: What is Redis pub/sub used for here?**
A: The C++ engine publishes trade events to a Redis channel whenever orders match. Any subscriber (Python strategy, monitoring tool, logger) receives those events in real time without the C++ engine needing to know about them.

**Q: Why Redis instead of just calling Python directly from C++?**
A: Decoupling. The C++ engine shouldn't care who is listening. Redis lets you add or remove subscribers without changing the engine. It also works across processes and machines.
