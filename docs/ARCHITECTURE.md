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

---

#### Deep Dive: Why We Chose `std::map` Over Alternatives

To truly understand our choice, let's walk through how the matching algorithm would work with each data structure option. We'll use a concrete example:

**Scenario**: We have an order book with these resting BID orders:
```
Price $150.00 → Order A (100 shares)
Price $149.50 → Order B (200 shares)
Price $149.00 → Order C (150 shares)
Price $148.50 → Order D (50 shares)
```

An incoming SELL order arrives: **SELL 250 shares @ $149.00 (limit)**

The matching engine must:
1. Find the best bid (highest price)
2. Match against it if price crosses (bid >= sell price)
3. Repeat until the sell order is filled or no more crossing bids

---

#### Option 1: `std::map` (Our Choice) ✓

```cpp
std::map<Price, PriceLevel, std::greater<Price>> bids_;
// Internal structure (red-black tree, sorted descending):
//
//           [149.50]
//           /      \
//      [150.00]  [149.00]
//                      \
//                   [148.50]
//
// Iteration order: 150.00 → 149.50 → 149.00 → 148.50
```

**How matching works:**

```cpp
void match_sell_order(Order& sell) {
    auto it = bids_.begin();  // O(1) - points to $150.00 (highest)

    while (sell.remaining() > 0 && it != bids_.end()) {
        Price bid_price = it->first;          // $150.00

        // Does the price cross? (bid >= sell limit)
        if (bid_price < sell.price) break;    // $150.00 >= $149.00 ✓

        // Match at this price level
        PriceLevel& level = it->second;
        // ... fill orders at this level

        ++it;  // O(1) - move to next price ($149.50)
    }
}
```

**Step-by-step execution:**

| Step | Best Bid | Action | Sell Remaining |
|------|----------|--------|----------------|
| 1 | $150.00 | `begin()` → $150.00. Cross? Yes (150 >= 149). Fill 100 from A. | 150 |
| 2 | $149.50 | `++it` → $149.50. Cross? Yes (149.50 >= 149). Fill 150 from B. | 0 |
| 3 | Done | Sell fully filled. | 0 |

**Performance:**
- Find best bid: O(1)
- Move to next level: O(1) amortized
- Insert new level: O(log n)
- Total for this match: O(k) where k = number of levels touched

---

#### Option 2: `std::unordered_map` ✗

```cpp
std::unordered_map<Price, PriceLevel> bids_;
// Internal structure (hash table, NO ORDER):
//
// Bucket 0: [149.00 → PriceLevel]
// Bucket 1: (empty)
// Bucket 2: [148.50 → PriceLevel]
// Bucket 3: [150.00 → PriceLevel]
// Bucket 4: [149.50 → PriceLevel]
//
// Iteration order: UNDEFINED (depends on hash function)
// Could be: 149.00 → 148.50 → 150.00 → 149.50 (useless!)
```

**How matching would have to work:**

```cpp
void match_sell_order(Order& sell) {
    // PROBLEM: We need the highest bid, but unordered_map has no order!

    // Option A: Scan ALL prices to find max (BAD)
    Price best_bid = 0;
    for (auto& [price, level] : bids_) {  // O(n) scan every time!
        if (price > best_bid) best_bid = price;
    }
    // Now we have $150.00, but we wasted O(n) operations

    // After matching at $150.00, we need the next best...
    // We have to scan ALL remaining prices again! Another O(n)
}
```

**Step-by-step execution:**

| Step | Action | Complexity |
|------|--------|------------|
| 1 | Scan all 4 prices to find max ($150.00) | O(n) = O(4) |
| 2 | Match at $150.00, remove it | O(1) |
| 3 | Scan remaining 3 prices to find max ($149.50) | O(n) = O(3) |
| 4 | Match at $149.50, partially fill | O(1) |
| 5 | Done | |

**Performance:**
- Find best bid: O(n) every time!
- Total for this match: O(n × k) where k = levels touched
- With 1000 price levels and 10 matches: 10,000 operations vs map's ~10

**Why this is a dealbreaker:**
```
Order book with 1000 price levels, matching 50 orders/second:

std::map:      50 × O(log 1000) ≈ 50 × 10 = 500 operations/second
unordered_map: 50 × O(1000)     = 50 × 1000 = 50,000 operations/second

That's 100x slower for the core matching loop!
```

---

#### Option 3: `std::vector<std::pair<Price, PriceLevel>>` (Sorted) ✗

```cpp
std::vector<std::pair<Price, PriceLevel>> bids_;  // Kept sorted descending
// Internal structure (contiguous memory):
//
// Index:  [0]       [1]       [2]       [3]
// Data:   $150.00   $149.50   $149.00   $148.50
//            ↑
//         Best bid (always index 0)
```

**How matching works (looks good!):**

```cpp
void match_sell_order(Order& sell) {
    size_t i = 0;  // Start at best bid

    while (sell.remaining() > 0 && i < bids_.size()) {
        Price bid_price = bids_[i].first;     // O(1) - nice!

        if (bid_price < sell.price) break;

        // Match at this level...

        if (level_empty) {
            bids_.erase(bids_.begin() + i);   // O(n) - PROBLEM!
        } else {
            ++i;  // O(1)
        }
    }
}
```

**The hidden cost - INSERTION:**

```cpp
// New order arrives: BUY 100 @ $149.75
// Need to insert between $150.00 and $149.50

void add_order(Order& buy) {
    Price price = buy.price;  // $149.75

    // Find insertion point (binary search) - O(log n), good!
    auto it = std::lower_bound(bids_.begin(), bids_.end(), price, ...);

    // Insert at that position - O(n), BAD!
    bids_.insert(it, {price, PriceLevel(...)});

    // What happens internally:
    // Before: [$150.00] [$149.50] [$149.00] [$148.50]
    //                      ↑ insert here
    // Must shift: [$149.50] [$149.00] [$148.50] all move right
    // After:  [$150.00] [$149.75] [$149.50] [$149.00] [$148.50]
}
```

**Step-by-step for insertion at index 1:**

| Step | Memory State | Operations |
|------|--------------|------------|
| Before | `[150][149.5][149][148.5][____]` | |
| 1. Shift [148.5] right | `[150][149.5][149][____][148.5]` | 1 copy |
| 2. Shift [149] right | `[150][149.5][____][149][148.5]` | 1 copy |
| 3. Shift [149.5] right | `[150][____][149.5][149][148.5]` | 1 copy |
| 4. Insert [149.75] | `[150][149.75][149.5][149][148.5]` | 1 write |

**Performance comparison:**

| Operation | std::map | Sorted vector |
|-----------|----------|---------------|
| Find best price | O(1) | O(1) |
| Insert new price level | O(log n) | O(n) |
| Remove price level | O(log n) | O(n) |
| Memory locality | Poor (tree nodes scattered) | Excellent (contiguous) |

**When vector wins:** If you have very few price levels (<50) and rarely insert new ones, the cache-friendly memory layout can beat map. But order books typically have 100s-1000s of levels with frequent insertions.

**Real numbers (1000 price levels):**
```
Insert new price level:
  std::map:      ~10 pointer operations (tree rebalance)
  sorted vector: ~500 element copies on average (shift half the array)
```

---

#### Option 4: `std::deque<std::pair<Price, PriceLevel>>` (Sorted) ✗

```cpp
std::deque<std::pair<Price, PriceLevel>> bids_;
// Internal structure (chunked arrays):
//
// Chunk 0: [$150.00] [$149.50] [$149.00] [$148.50]
// Chunk 1: [$148.00] [$147.50] [$147.00] [$146.50]
// ...
```

**Deque's advantage**: O(1) insertion/removal at BOTH ends (front and back).

**But we need middle insertion!**

```cpp
// New order: BUY @ $149.25 (goes between $149.50 and $149.00)
auto it = std::lower_bound(...);  // Find position
bids_.insert(it, {...});          // Still O(n)! Middle insertion is slow.
```

**Deque is good for:**
- Queue operations (push_back, pop_front)
- Stack operations (push_back, pop_back)

**Deque is NOT good for:**
- Sorted containers with arbitrary insertion (our use case)

---

#### Option 5: Skip List (Alternative worth mentioning)

```
Level 3:  [HEAD] ─────────────────────────────────→ [$148.50] → [TAIL]
Level 2:  [HEAD] ───────────→ [$149.50] ──────────→ [$148.50] → [TAIL]
Level 1:  [HEAD] → [$150.00] → [$149.50] → [$149.00] → [$148.50] → [TAIL]
Level 0:  [HEAD] → [$150.00] → [$149.50] → [$149.00] → [$148.50] → [TAIL]
              ↑
           Best bid
```

**How it works:**
- Multiple linked lists stacked on top of each other
- Higher levels "skip" over elements for faster traversal
- Probabilistic balancing (no rotations needed like red-black tree)

**Performance:**
| Operation | Skip List | std::map |
|-----------|-----------|----------|
| Find best | O(1) | O(1) |
| Insert | O(log n) average | O(log n) worst case |
| Memory | More (extra pointers) | Less |

**Why we didn't choose it:**
- `std::map` is in the standard library - no external dependencies
- Skip list requires custom implementation (more code to maintain)
- Performance is comparable; map has stronger worst-case guarantees
- For learning purposes, understanding red-black trees via `std::map` is more transferable

**When skip list wins:** High-concurrency scenarios. Skip lists are easier to make lock-free because they don't need tree rotations (which require locking multiple nodes).

---

#### Summary: Data Structure Comparison

| Criteria | `std::map` | `unordered_map` | Sorted `vector` | `deque` | Skip List |
|----------|:----------:|:---------------:|:---------------:|:-------:|:---------:|
| Find best price | O(1) ✓ | O(n) ✗ | O(1) ✓ | O(1) ✓ | O(1) ✓ |
| Insert new level | O(log n) ✓ | O(1) ✓ | O(n) ✗ | O(n) ✗ | O(log n) ✓ |
| Remove level | O(log n) ✓ | O(1) ✓ | O(n) ✗ | O(n) ✗ | O(log n) ✓ |
| Iterate in order | O(1)/step ✓ | O(n log n)* ✗ | O(1)/step ✓ | O(1)/step ✓ | O(1)/step ✓ |
| Standard library | ✓ | ✓ | ✓ | ✓ | ✗ |
| Cache locality | Poor | Poor | Excellent | Good | Poor |

*Must sort keys first

**Our choice: `std::map`** — Best balance of:
- Correct semantics (sorted traversal)
- Good performance (O(log n) operations)
- Standard library (no dependencies)
- Predictable behavior (balanced tree guarantees)

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
