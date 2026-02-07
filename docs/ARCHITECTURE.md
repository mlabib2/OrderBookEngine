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

**Internal structure** (red-black tree, sorted descending):

```
           [149.50]
           /      \
      [150.00]  [149.00]
                      \
                   [148.50]

Iteration order: 150.00 → 149.50 → 149.00 → 148.50
```

**How matching works:**

1. Call `begin()` → get iterator to $150.00 (highest bid) in O(1)
2. Check: Does price cross? ($150.00 >= $149.00) ✓ Yes, match!
3. Call `++iterator` → move to $149.50 in O(1)
4. Check: Does price cross? ($149.50 >= $149.00) ✓ Yes, match!
5. Order filled, done.

**Step-by-step execution:**

| Step | Best Bid | Action                                                | Sell Remaining |
| ---- | -------- | ----------------------------------------------------- | -------------- |
| 1    | $150.00  | Get best via `begin()`. Cross? Yes. Fill 100 from A.  | 150            |
| 2    | $149.50  | Move to next via `++it`. Cross? Yes. Fill 150 from B. | 0              |
| 3    | Done     | Sell fully filled.                                    | 0              |

**Performance:**

- Find best bid: O(1)
- Move to next level: O(1) amortized
- Insert new level: O(log n)
- Total for this match: O(k) where k = number of levels touched

---

#### Option 2: `std::unordered_map` ✗

**Internal structure** (hash table, NO ORDER):

```
Bucket 0: [149.00 → PriceLevel]
Bucket 1: (empty)
Bucket 2: [148.50 → PriceLevel]
Bucket 3: [150.00 → PriceLevel]
Bucket 4: [149.50 → PriceLevel]

Iteration order: UNDEFINED (depends on hash function)
Could be: 149.00 → 148.50 → 150.00 → 149.50 (useless for matching!)
```

**The problem:**

We need the highest bid, but `unordered_map` has no concept of order. To find the best bid, we must scan ALL prices every single time.

**How matching would have to work:**

1. Scan all 4 prices to find max → find $150.00 (O(n) operations!)
2. Match at $150.00, remove it
3. Scan remaining 3 prices to find max → find $149.50 (O(n) again!)
4. Match at $149.50
5. Done

**Step-by-step execution:**

| Step | Action                                        | Complexity  |
| ---- | --------------------------------------------- | ----------- |
| 1    | Scan all 4 prices to find max ($150.00)       | O(n) = O(4) |
| 2    | Match at $150.00, remove it                   | O(1)        |
| 3    | Scan remaining 3 prices to find max ($149.50) | O(n) = O(3) |
| 4    | Match at $149.50, partially fill              | O(1)        |
| 5    | Done                                          |             |

**Performance:**

- Find best bid: O(n) every time!
- Total for this match: O(n × k) where k = levels touched

**Why this is a dealbreaker:**

```
Order book with 1000 price levels, matching 50 orders/second:

std::map:      50 × O(1) per level = ~50 operations for best price lookups
unordered_map: 50 × O(1000) scans  = ~50,000 operations for best price lookups

That's 1000x more work for the core matching loop!
```

---

#### Option 3: Sorted `std::vector` ✗

**Internal structure** (contiguous memory, manually kept sorted):

```
Index:  [0]       [1]       [2]       [3]
Data:   $150.00   $149.50   $149.00   $148.50
           ↑
        Best bid (always index 0)
```

**Matching looks good!**

- Access best bid at index 0: O(1) ✓
- Move to next: increment index: O(1) ✓

**But the hidden cost is INSERTION:**

New order arrives: BUY 100 @ $149.75 (needs to go between $150.00 and $149.50)

```
Before: [$150.00] [$149.50] [$149.00] [$148.50]
                     ↑ insert here

Step 1: Shift [$148.50] right → [$150.00] [$149.50] [$149.00] [______] [$148.50]
Step 2: Shift [$149.00] right → [$150.00] [$149.50] [______] [$149.00] [$148.50]
Step 3: Shift [$149.50] right → [$150.00] [______] [$149.50] [$149.00] [$148.50]
Step 4: Insert [$149.75]      → [$150.00] [$149.75] [$149.50] [$149.00] [$148.50]

Total: 3 element copies + 1 write = O(n) work!
```

**Performance comparison:**

| Operation              | std::map         | Sorted vector          |
| ---------------------- | ---------------- | ---------------------- |
| Find best price        | O(1)             | O(1)                   |
| Insert new price level | O(log n)         | O(n)                   |
| Remove price level     | O(log n)         | O(n)                   |
| Memory locality        | Poor (scattered) | Excellent (contiguous) |

**When vector wins:** Very few price levels (<50) with rare insertions. The cache-friendly memory layout can beat map's pointer chasing.

**When vector loses:** Order books with 100s-1000s of levels and frequent insertions (our case).

**Real numbers (1000 price levels):**

```
Insert new price level:
  std::map:      ~10 pointer operations (tree rebalance)
  sorted vector: ~500 element copies on average (shift half the array)
```

---

#### Option 4: Sorted `std::deque` ✗

**Internal structure** (chunked arrays):

```
Chunk 0: [$150.00] [$149.50] [$149.00] [$148.50]
Chunk 1: [$148.00] [$147.50] [$147.00] [$146.50]
...
```

**Deque's advantage**: O(1) insertion/removal at BOTH ends (front and back).

**But we need middle insertion!**

New order at $149.25 goes between $149.50 and $149.00 — this is a middle insertion, which is still O(n).

**Deque is good for:**

- Queue operations (push_back, pop_front)
- Stack operations (push_back, pop_back)

**Deque is NOT good for:**

- Sorted containers with arbitrary insertion (our use case)

---

#### Option 5: Skip List

**Internal structure** (multi-level linked list):

```
Level 3:  [HEAD] ─────────────────────────────────→ [$148.50] → [TAIL]
Level 2:  [HEAD] ───────────→ [$149.50] ──────────→ [$148.50] → [TAIL]
Level 1:  [HEAD] → [$150.00] → [$149.50] → [$149.00] → [$148.50] → [TAIL]
Level 0:  [HEAD] → [$150.00] → [$149.50] → [$149.00] → [$148.50] → [TAIL]
              ↑
           Best bid (follow Level 0 from HEAD)
```

**How it works:**

- Multiple linked lists stacked on top of each other
- Higher levels "skip" over elements for faster traversal
- Probabilistic balancing (flip coins to decide how many levels each node gets)

**Performance comparison:**

| Operation | Skip List                   | std::map            |
| --------- | --------------------------- | ------------------- |
| Find best | O(1)                        | O(1)                |
| Insert    | O(log n) average            | O(log n) guaranteed |
| Memory    | More (extra level pointers) | Less                |

**Why we didn't choose it:**

- `std::map` is in the standard library — no external dependencies
- Skip list requires custom implementation (more code, more bugs)
- Performance is comparable; map has stronger worst-case guarantees
- Understanding red-black trees via `std::map` is more transferable knowledge

**When skip list wins:** High-concurrency scenarios. Skip lists are easier to make lock-free because insertions don't require tree rotations (which would lock multiple nodes).

---

#### Summary: Price Level Data Structure Comparison

| Criteria         | `std::map`  |  `unordered_map`  | Sorted `vector` |   `deque`   |  Skip List  |
| ---------------- | :---------: | :---------------: | :-------------: | :---------: | :---------: |
| Find best price  |   O(1) ✓    |      O(n) ✗       |     O(1) ✓      |   O(1) ✓    |   O(1) ✓    |
| Insert new level | O(log n) ✓  |      O(1) ✓       |     O(n) ✗      |   O(n) ✗    | O(log n) ✓  |
| Remove level     | O(log n) ✓  |      O(1) ✓       |     O(n) ✗      |   O(n) ✗    | O(log n) ✓  |
| Iterate in order | O(1)/step ✓ | Must sort first ✗ |   O(1)/step ✓   | O(1)/step ✓ | O(1)/step ✓ |
| Standard library |      ✓      |         ✓         |        ✓        |      ✓      |      ✗      |
| Cache locality   |    Poor     |       Poor        |    Excellent    |    Good     |    Poor     |

**Our choice: `std::map`** — Best balance of correct semantics, good performance, and standard library availability.

---

### 2. Order Queue at Each Price: `std::list`

```
PriceLevel {
    price_: Price
    total_quantity_: Quantity
    orders_: std::list<Order*>  // FIFO queue
}
```

**Why `std::list`?**

- **FIFO order**: First order in = first order matched (time priority / fairness).
- **O(1) front access**: Get the first order instantly for matching.
- **O(1) removal anywhere**: If we store the iterator, we can remove any order in O(1).
- **No iterator invalidation**: Other iterators stay valid when we remove an order.

---

#### Deep Dive: Why We Chose `std::list` Over Alternatives

**Scenario**: At price level $150.00, we have these orders (in arrival order):

```
Order A (100 shares) → Order B (50 shares) → Order C (75 shares) → Order D (200 shares)
  ↑ arrived first                                                      ↑ arrived last
```

We need to support two operations:

1. **Match**: Fill orders from the front (FIFO — Order A gets filled first)
2. **Cancel**: Remove any order by ID (user cancels Order C in the middle)

---

#### Option 1: `std::list` (Our Choice) ✓

**Internal structure** (doubly-linked list):

```
HEAD ←→ [Order A] ←→ [Order B] ←→ [Order C] ←→ [Order D] ←→ TAIL
            ↑                          ↑
         front()                   (stored iterator for O(1) cancel)
```

**Matching flow** (fill incoming sell for 125 shares):

| Step | Front Order   | Action                                 | Remaining to Fill |
| ---- | ------------- | -------------------------------------- | ----------------- |
| 1    | Order A (100) | `front()` → A. Fill 100. Remove A.     | 25                |
| 2    | Order B (50)  | `front()` → B. Fill 25. B has 25 left. | 0                 |
| 3    | Done          |                                        |                   |

**Cancel flow** (cancel Order C):

```
Before: HEAD ←→ [A] ←→ [B] ←→ [C] ←→ [D] ←→ TAIL
                              ↑
                         iterator to C (stored when C was added)

After:  HEAD ←→ [A] ←→ [B] ←→ [D] ←→ TAIL

Just update two pointers:
  - B.next = D
  - D.prev = B
Done in O(1)!
```

**Key insight**: When we add Order C, we store its list iterator in our lookup map. Later, we use that iterator to erase C directly — no searching needed.

**Iterator stability**: After removing C, iterators to A, B, and D are still valid. This is critical because we might be in the middle of matching when a cancel arrives.

---

#### Option 2: `std::vector` ✗

**Internal structure** (contiguous array):

```
Index:    [0]        [1]        [2]        [3]
Data:   Order A    Order B    Order C    Order D
           ↑
        front (index 0)
```

**Matching is O(n)!**

After filling Order A, we need to remove it from the front:

```
Before: [A] [B] [C] [D]
         ↑ remove this

Step 1: Shift B left → [B] [B] [C] [D]
Step 2: Shift C left → [B] [C] [C] [D]
Step 3: Shift D left → [B] [C] [D] [D]
Step 4: Shrink size  → [B] [C] [D]

Total: 3 copies = O(n)
```

**Cancel is also O(n)!**

Removing Order C from the middle requires shifting D:

```
Before: [A] [B] [C] [D]
                 ↑ remove this

After:  [A] [B] [D]
              ↑ shifted left
```

**Iterator invalidation problem:**

If we're iterating through the vector and someone cancels an order, our iterator becomes invalid (elements shifted). This creates race conditions and bugs.

**Performance comparison:**

| Operation              | std::list              | std::vector               |
| ---------------------- | ---------------------- | ------------------------- |
| Get front order        | O(1)                   | O(1)                      |
| Remove from front      | O(1)                   | O(n) — shift all elements |
| Remove from middle     | O(1) with iterator     | O(n) — shift elements     |
| Iterator after removal | Still valid ✓          | Invalidated ✗             |
| Memory locality        | Poor (scattered nodes) | Excellent (contiguous)    |

---

#### Option 3: `std::deque` ✗

**Internal structure** (chunked arrays):

```
Chunk 0: [Order A] [Order B]
Chunk 1: [Order C] [Order D]
```

**Front operations are O(1)!** ✓

Deque is optimized for push/pop at both ends. Removing Order A from the front is O(1).

**But middle removal is O(n)!** ✗

Canceling Order C still requires shifting within the chunk:

```
Before Chunk 1: [Order C] [Order D]
                    ↑ remove

After Chunk 1:  [Order D]
                    ↑ shifted
```

**Comparison:**

| Operation              | std::list | std::deque    |
| ---------------------- | --------- | ------------- |
| Remove from front      | O(1) ✓    | O(1) ✓        |
| Remove from middle     | O(1) ✓    | O(n) ✗        |
| Iterator after removal | Valid ✓   | Invalidated ✗ |

**Deque would work if** we never canceled orders. But order cancellation is a core feature (users cancel ~30-50% of orders in real markets).

---

#### Option 4: `std::queue` ✗

`std::queue` is just a wrapper around `std::deque` (by default) that only exposes `push()`, `pop()`, `front()`, and `back()`.

**Fatal flaw**: No way to remove from the middle. We cannot implement order cancellation at all.

---

#### Summary: Order Queue Data Structure Comparison

| Criteria             | `std::list` | `std::vector` | `std::deque`  | `std::queue` |
| -------------------- | :---------: | :-----------: | :-----------: | :----------: |
| Get front (matching) |   O(1) ✓    |    O(1) ✓     |    O(1) ✓     |    O(1) ✓    |
| Remove front         |   O(1) ✓    |    O(n) ✗     |    O(1) ✓     |    O(1) ✓    |
| Cancel from middle   |   O(1) ✓    |    O(n) ✗     |    O(n) ✗     | Impossible ✗ |
| Iterator stability   |  Stable ✓   | Invalidated ✗ | Invalidated ✗ |     N/A      |
| Cache locality       |    Poor     |   Excellent   |     Good      |     Good     |

**Our choice: `std::list`** — The only option that supports O(1) cancellation from any position with iterator stability.

---

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

---

#### Deep Dive: Why We Chose `std::unordered_map` Over Alternatives

**Scenario**: We have 10,000 active orders in the book. A cancel request arrives:

```
CANCEL Order ID: 7842
```

We need to:

1. Find where Order 7842 is located (which side? which price level? which position in queue?)
2. Remove it from the price level's order queue
3. Update the price level's total quantity

---

#### Option 1: `std::unordered_map` (Our Choice) ✓

**Internal structure** (hash table):

```
Hash of 7842 = bucket 42

Bucket 0:  [1001 → LocationA]
Bucket 1:  (empty)
...
Bucket 42: [7842 → LocationX] → [3042 → LocationY]  (chain for collisions)
...
Bucket 99: [5501 → LocationZ]
```

**Cancel flow:**

| Step      | Operation                          | Complexity                  |
| --------- | ---------------------------------- | --------------------------- |
| 1         | Hash 7842 → bucket 42              | O(1)                        |
| 2         | Find entry in bucket               | O(1) average (short chains) |
| 3         | Get stored iterator from LocationX | O(1)                        |
| 4         | Use iterator to erase from list    | O(1)                        |
| **Total** |                                    | **O(1)**                    |

**What we store in LocationX:**

```
LocationX = {
    side: BUY,
    price: 15050000,  // $150.50 in fixed-point
    iterator: ──────→ points directly to Order 7842 in the list
    order_ptr: ────→ pointer to Order 7842 data
}
```

The iterator is the magic — it lets us jump directly to the order's position in the linked list without traversing.

---

#### Option 2: `std::map` ✗

**Internal structure** (red-black tree, sorted by OrderId):

```
              [5000]
             /      \
         [2500]    [7500]
         /    \        \
     [1001]  [3042]   [7842]  ← Order we want
```

**Cancel flow:**

| Step      | Operation             | Complexity   |
| --------- | --------------------- | ------------ |
| 1         | Start at root [5000]  |              |
| 2         | 7842 > 5000, go right | O(1)         |
| 3         | 7842 > 7500, go right | O(1)         |
| 4         | Found [7842]          |              |
| **Total** |                       | **O(log n)** |

**With 10,000 orders:**

```
std::unordered_map: ~1-2 operations (hash + bucket lookup)
std::map:           ~13 operations (log₂(10000) ≈ 13 tree traversals)
```

**With 1,000,000 orders:**

```
std::unordered_map: ~1-2 operations (still constant!)
std::map:           ~20 operations (log₂(1000000) ≈ 20)
```

**Why O(log n) matters for cancel:**

- Target: <1000 nanoseconds for cancel
- Each tree traversal: ~5-10 nanoseconds (cache miss on each node)
- 20 traversals × 10ns = 200ns (acceptable)
- But with O(1): ~20ns (10x faster!)

In high-frequency trading, this 10x difference is significant.

---

#### Option 3: Linear Search (No Lookup Map) ✗

Without any lookup structure, we'd have to search:

```
For each price level in bids:        ← O(p) price levels
    For each order in that level:    ← O(q) orders per level
        If order.id == 7842: found!

Total: O(p × q) = O(n) where n = total orders
```

**With 10,000 orders:**

```
Average case: scan 5,000 orders to find the one we want
At ~5ns per comparison: 25,000 nanoseconds = 25 microseconds

Target was <1000 nanoseconds. We're 25x too slow!
```

---

#### The Iterator Trick Explained

When we add an order, we do this:

```
1. Insert order into price level's list
   list.push_back(order)  → returns iterator

2. Store that iterator in our lookup map
   lookup[order.id] = {side, price, iterator, &order}
```

When we cancel:

```
1. Find the LocationX in lookup map    → O(1)
2. Get the stored iterator             → O(1)
3. Call list.erase(iterator)           → O(1)
```

Without storing the iterator, step 3 would require:

```
3. Search through the list for the order  → O(q)
   for (auto it = list.begin(); it != list.end(); ++it) {
       if ((*it)->id == 7842) {
           list.erase(it);
           break;
       }
   }
```

This changes cancel from O(1) to O(q) where q = orders at that price level.

---

#### Summary: Order Lookup Data Structure Comparison

| Criteria           | `std::unordered_map` |   `std::map`   |  No lookup (scan)   |
| ------------------ | :------------------: | :------------: | :-----------------: |
| Find order by ID   |        O(1) ✓        |    O(log n)    |       O(n) ✗        |
| Memory overhead    | Hash table + entries |   Tree nodes   |        None         |
| With 10,000 orders |    ~2 operations     | ~13 operations |  ~5,000 operations  |
| With 1M orders     |    ~2 operations     | ~20 operations | ~500,000 operations |

**Our choice: `std::unordered_map`** — O(1) lookup is essential for meeting our <1000ns cancel target.

---

#### The Complete Cancel Flow

Putting it all together, here's how a cancel works end-to-end:

```
CANCEL Order 7842

Step 1: Lookup
  └─→ order_lookup_[7842] → {BUY, $150.50, iterator, ptr}     O(1)

Step 2: Access price level
  └─→ bids_[$150.50] → PriceLevel                             O(log n)

Step 3: Remove from queue using stored iterator
  └─→ level.orders_.erase(iterator)                           O(1)

Step 4: Update level quantity
  └─→ level.total_quantity_ -= order.remaining()              O(1)

Step 5: Remove from lookup
  └─→ order_lookup_.erase(7842)                               O(1)

Step 6: (Optional) Remove empty price level
  └─→ if level.empty(): bids_.erase($150.50)                  O(log n)

Total: O(log n) — dominated by map access, not the lookup or list removal
```

The O(1) unordered_map lookup + O(1) list erase ensures that the only logarithmic cost is the price level map access, which we can't avoid.

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

| Operation                  | Complexity     | How                            |
| -------------------------- | -------------- | ------------------------------ |
| Add order (no match)       | O(log n)       | Map insertion                  |
| Add order (with k matches) | O(log n + k)   | Map insert + k list operations |
| Cancel order               | O(1) amortized | Hash lookup + iterator erase   |
| Best bid/ask               | O(1)           | `map.begin()`                  |
| Spread                     | O(1)           | Two `map.begin()` calls        |
| Book depth (d levels)      | O(d)           | Iterate d map entries          |

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
