# Architecture: Order Book Engine

## Overview

This document explains the data structures and algorithms used in the order book engine, and WHY each choice was made.

---

## Core Data Structures

### 1. Price Levels: `std::map`

```
bids_: std::map<Price, PriceLevel, std::greater<Price>>  // Descending order
asks_: std::map<Price, PriceLevel, std::less<Price>>     // Ascending order
```

**Why `std::map`?**
- **Sorted**: Prices are automatically sorted. Bids: highest first. Asks: lowest first.
- **O(1) best price**: `map.begin()` gives us the best bid/ask instantly.
- **O(log n) insert**: Adding a new price level is logarithmic.
- **Red-black tree**: Balanced, predictable performance.

**Why not `std::unordered_map`?**
- Unordered maps don't maintain sort order.
- We'd have to scan all prices to find the best one - O(n) instead of O(1).

### 2. Order Queue at Each Price: `std::list`

```
PriceLevel {
    price_: Price
    total_quantity_: Quantity
    orders_: std::list<Order*>  // FIFO queue
}
```

**Why `std::list`?**
- **FIFO order**: First order in = first order matched. This is fairness (time priority).
- **O(1) front access**: Get the first order instantly for matching.
- **O(1) removal anywhere**: If we store the iterator, we can remove any order in O(1).
- **No iterator invalidation**: Other iterators stay valid when we remove an order.

**Why not `std::vector`?**
- Removing from the front is O(n) - have to shift all elements.
- Iterator invalidation when removing elements.

**Why not `std::deque`?**
- Could work, but removal from middle is still O(n).
- We need to cancel orders that aren't at the front.

### 3. Order Lookup: `std::unordered_map`

```
order_lookup_: std::unordered_map<OrderId, OrderLocation>

OrderLocation {
    side: Side
    price: Price
    order_iterator: std::list<Order*>::iterator
    order_ptr: Order*
}
```

**Why `std::unordered_map`?**
- **O(1) lookup**: Find any order by ID instantly.
- **O(1) cancel**: Combined with stored iterator, we can cancel in O(1).

**The key insight**: We store the `std::list` iterator when we add an order. Later, we can use that iterator to remove the order from the list in O(1), without searching.

---

## Visual Representation

```
ORDER BOOK FOR "AAPL"

BIDS (sorted descending)              ASKS (sorted ascending)
┌─────────────────────────────┐      ┌─────────────────────────────┐
│ Price: 150.50               │      │ Price: 150.75               │ <- Best Ask
│ Orders: [A:100] -> [B:50]   │      │ Orders: [G:200]             │
├─────────────────────────────┤      ├─────────────────────────────┤
│ Price: 150.25               │      │ Price: 151.00               │
│ Orders: [C:75]              │      │ Orders: [H:100] -> [I:150]  │
├─────────────────────────────┤      ├─────────────────────────────┤
│ Price: 150.00               │ <-   │ Price: 151.50               │
│ Orders: [D:200] -> [E:100]  │Best  │ Orders: [J:50]              │
│         -> [F:50]           │ Bid  │                             │
└─────────────────────────────┘      └─────────────────────────────┘

SPREAD = 150.75 - 150.50 = 0.25

ORDER LOOKUP (hash map)
┌──────────────┬─────────────────────────────────────┐
│ Order ID     │ Location                            │
├──────────────┼─────────────────────────────────────┤
│ A            │ {BUY, 150.50, iterator_to_A, &A}    │
│ B            │ {BUY, 150.50, iterator_to_B, &B}    │
│ C            │ {BUY, 150.25, iterator_to_C, &C}    │
│ ...          │ ...                                 │
│ G            │ {SELL, 150.75, iterator_to_G, &G}   │
└──────────────┴─────────────────────────────────────┘
```

---

## Matching Algorithm: Price-Time Priority

### The Rules

1. **Price Priority**: Better price matches first
   - For a SELL order: highest BID matches first
   - For a BUY order: lowest ASK matches first

2. **Time Priority**: At the same price, earlier orders match first (FIFO)

### Algorithm Pseudocode

```
match_order(incoming_order):
    opposite_book = (incoming is BUY) ? asks : bids
    trades = []

    while incoming has remaining quantity AND opposite_book not empty:
        best_level = opposite_book.first()  // O(1)

        // Check if prices cross
        if BUY and incoming.price < best_level.price: break
        if SELL and incoming.price > best_level.price: break

        // Match against orders at this level (FIFO)
        while level has orders AND incoming has remaining:
            resting = level.front()  // First order (time priority)

            fill_qty = min(incoming.remaining, resting.remaining)
            create_trade(incoming, resting, fill_qty)

            update_quantities()
            if resting is filled: remove_from_book(resting)

        if level is empty: remove_price_level()

    if incoming has remaining: add_to_book(incoming)
    return trades
```

---

## Complexity Analysis

| Operation | Complexity | How |
|-----------|------------|-----|
| Add order (no match) | O(log n) | Map insertion |
| Add order (with k matches) | O(log n + k) | Map insert + k list operations |
| Cancel order | O(1) amortized | Hash lookup + iterator erase |
| Best bid/ask | O(1) | `map.begin()` |
| Spread | O(1) | Two `map.begin()` calls |
| Book depth (d levels) | O(d) | Iterate d map entries |

Where n = number of price levels, k = number of trades generated.

---

## Memory Layout

### Order Structure (~80 bytes)
```cpp
struct Order {
    OrderId id;              // 8 bytes
    Price price;             // 8 bytes (int64_t)
    Quantity quantity;       // 8 bytes
    Quantity filled_qty;     // 8 bytes
    Side side;               // 1 byte
    OrderType type;          // 1 byte
    OrderStatus status;      // 1 byte
    // padding              // 5 bytes
    Timestamp timestamp;     // 8 bytes
    std::string symbol;      // 32 bytes (SSO)
    // Total: ~80 bytes (well under 200 byte target)
};
```

### Why Fixed-Point Prices?

```cpp
// BAD: Floating point
double price1 = 0.1 + 0.2;  // = 0.30000000000000004 (not 0.3!)
if (price1 == 0.3) { ... }  // FALSE! Bug!

// GOOD: Fixed-point integer
int64_t price1 = 100000 + 200000;  // = 300000 (exactly 0.3 with 6 decimals)
if (price1 == 300000) { ... }      // TRUE! Correct!

// Conversion
constexpr int64_t PRICE_MULTIPLIER = 1'000'000;
double to_double(int64_t price) { return price / (double)PRICE_MULTIPLIER; }
int64_t to_fixed(double price) { return (int64_t)(price * PRICE_MULTIPLIER); }
```

---

## Error Handling Strategy

We use error codes instead of exceptions for performance:

```cpp
enum class ErrorCode : uint8_t {
    Success = 0,
    OrderNotFound,
    InvalidQuantity,
    InvalidPrice,
    InsufficientLiquidity
};

// Fast path - no exception overhead
ErrorCode cancel_order(OrderId id) {
    auto it = order_lookup_.find(id);
    if (it == order_lookup_.end()) {
        return ErrorCode::OrderNotFound;  // No exception throw
    }
    // ... cancel logic
    return ErrorCode::Success;
}
```

**Why not exceptions?**
- Throwing an exception can take 1000+ nanoseconds
- Our target is <1000 nanoseconds for cancel
- Error codes are just integer comparisons - essentially free
