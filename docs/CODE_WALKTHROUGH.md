# Code Walkthrough: Understanding Every File

This document walks through every file in the codebase, line by line.
No assumed knowledge. If something is C++ syntax you haven't seen before, it's explained here.

---

## Table of Contents

1. [The Big Picture](#1-the-big-picture)
2. [C++ Syntax Primer](#2-c-syntax-primer)
3. [File: `types.hpp`](#3-file-typeshpp)
4. [File: `order.hpp`](#4-file-orderhpp)
5. [File: `trade.hpp`](#5-file-tradehpp)
6. [File: `price_level.hpp` + `price_level.cpp`](#6-file-price_levelhpp--price_levelcpp)
7. [File: `order_book.hpp` + `order_book.cpp`](#7-file-order_bookhpp--order_bookcpp)
8. [End-to-End Flow Diagrams](#8-end-to-end-flow-diagrams)

---

## 1. The Big Picture

Before reading any code, understand what these files ARE and how they relate to each other.

```
┌─────────────────────────────────────────────────────────────┐
│                       THE SYSTEM                            │
│                                                             │
│   You submit an order → Engine matches it → Trade happens   │
└─────────────────────────────────────────────────────────────┘

FILE DEPENDENCY TREE (each file uses the files above it):

  types.hpp          ← Primitive types. Everything else uses this.
      │
      ├── order.hpp  ← What an order looks like.
      │
      ├── trade.hpp  ← What a completed match looks like.
      │
      └── price_level.hpp / .cpp
              │      ← A container of orders at ONE price.
              │
          order_book.hpp / .cpp
                     ← The full book. Matching happens here.
```

### In Plain English

| File | Role | Analogy |
|------|------|---------|
| `types.hpp` | Defines what "Price", "Quantity", etc. even mean | Dictionary |
| `order.hpp` | A single buy/sell request | A slip of paper saying "buy 100 AAPL at $150" |
| `trade.hpp` | A completed deal between two orders | A receipt confirming a sale happened |
| `price_level.hpp` | A queue of orders at one price | A line of customers all offering the same price |
| `order_book.hpp` | The full book + matching engine | The entire market for one stock |

---

## 2. C++ Syntax Primer

These are the C++ patterns used throughout the code. Read this first if any syntax looks unfamiliar.

### `using X = SomeType` (Type Aliases)

```cpp
using OrderId = uint64_t;
```

This means: "whenever I write `OrderId`, treat it as `uint64_t`".
It's just a rename. `OrderId id = 5;` is exactly the same as `uint64_t id = 5;`.

**Why do this?** `OrderId` is more meaningful than `uint64_t`. The code reads like English.

---

### `enum class` (Named Choices)

```cpp
enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};
```

An `enum class` defines a fixed set of named options.
- `Side::Buy` is just the number `0` under the hood
- `Side::Sell` is just the number `1`
- `: uint8_t` means use 1 byte instead of 4 (saves memory)

You use it like: `Side s = Side::Buy;`

**Why `class` in `enum class`?** Without `class`, `Buy` and `Sell` would be global names and could clash with other code. With `class`, they're scoped to `Side::`.

---

### `constexpr` (Compile-Time Constants)

```cpp
constexpr int64_t PRICE_MULTIPLIER = 1'000'000;
```

`constexpr` means the value is computed at compile time, not runtime. Zero runtime cost.
The `'` in `1'000'000` is just a visual separator — it's the same as `1000000`.

---

### `inline` Functions

```cpp
inline Price price_to_fixed(double price) {
    return static_cast<Price>(price * PRICE_MULTIPLIER);
}
```

`inline` suggests to the compiler: "copy this function body wherever it's called, instead of doing a function call". This eliminates function-call overhead for tiny utility functions.

---

### `namespace` (Grouping Code)

```cpp
namespace orderbook {
    // all our code lives here
}
```

Namespaces prevent naming conflicts. If another library also has an `Order` struct, ours is `orderbook::Order` and theirs might be `somelib::Order`. No collision.

---

### `struct` vs `class`

Both define a type with members. The only difference: `struct` members are `public` by default, `class` members are `private` by default.

We use `struct` for plain data holders (`Order`, `Trade`) and `class` for things with logic that needs hiding (`PriceLevel`, `OrderBook`).

---

### `noexcept`

```cpp
bool is_filled() const noexcept { ... }
```

`noexcept` is a promise: "this function will never throw an exception."
This lets the compiler make optimizations and signals to callers that it's safe to call in performance-critical paths.

---

### `const` on Member Functions

```cpp
bool is_filled() const noexcept { ... }
```

The `const` after the parentheses means: "calling this function won't modify the object."
Non-`const` functions can modify the object. `const` functions can't.

---

### `std::optional<T>`

```cpp
std::optional<Price> best_bid() const noexcept;
```

An `optional<T>` is either:
- A value of type `T`, OR
- Empty (no value)

Like a box that's either holding something or is empty.

```cpp
auto bid = best_bid();
if (bid) {                      // Is there a value?
    double price = *bid;        // Unwrap it with *
}
```

We use it for `best_bid()` because the book might be empty — there IS no best bid.

---

### Initializer List in Constructors

```cpp
Order(OrderId id_, ...)
    : id(id_)         // ← this is the initializer list
    , price(price_)
    , quantity(quantity_)
{}
```

The `: field(value)` syntax initializes fields BEFORE the constructor body runs.
This is more efficient than assigning inside `{}`. Always prefer this for member initialization.

---

### Pointers vs Values

```cpp
std::list<Order*> orders_;   // list of POINTERS to Orders
Order* front() noexcept;     // returns a POINTER to an Order
```

`Order*` is a pointer — it stores the memory address of an Order, not the Order itself.
The PriceLevel stores pointers because the actual `Order` objects live in the MatchingEngine. The list just needs to reference them.

To access a field through a pointer: `order->id` (arrow notation).
To access directly: `order.id` (dot notation).

---

### Iterator Pattern

```cpp
using OrderIterator = std::list<Order*>::iterator;
```

An iterator is like a bookmark inside a container. It remembers the position of one element.

```cpp
auto it = list.begin();  // iterator pointing to first element
*it;                     // get the element at this position
++it;                    // move to next element
list.erase(it);          // remove the element at this position
```

Iterators are the key to our O(1) cancel: we store where each order is, so we can jump straight to it.

---

## 3. File: `types.hpp`

**Purpose**: Define the fundamental types that all other files use.

### The Include Guard

```cpp
#ifndef ORDERBOOK_TYPES_HPP
#define ORDERBOOK_TYPES_HPP
// ... all code ...
#endif
```

This pattern prevents the file from being included twice. If it's already been included, the `#define` is set and everything inside is skipped the second time.

### Type Aliases

```cpp
using OrderId  = uint64_t;   // 64-bit unsigned integer for order IDs
using TradeId  = uint64_t;   // same for trade IDs
using Price    = int64_t;    // 64-bit SIGNED integer (signed for arithmetic)
using Quantity = uint64_t;   // unsigned because you can't have -5 shares
using Timestamp = std::chrono::steady_clock::time_point;
```

`uint64_t` = unsigned 64-bit integer. Range: 0 to ~18 quintillion.
`int64_t`  = signed 64-bit integer. Range: ~-9.2 quintillion to ~9.2 quintillion.

### Why Price is an Integer (Fixed-Point)

```cpp
constexpr int64_t PRICE_MULTIPLIER = 1'000'000;
```

Prices look like $100.50, but we store them as `100500000` (multiply by 1,000,000).

**The problem with `double`:**

```
double a = 0.1 + 0.2;
// a is actually 0.30000000000000004 ← not 0.3!
// Comparing: a == 0.3 → FALSE
```

This is a fundamental limitation of how floating-point numbers work in binary.
In trading, a price comparison bug means orders that SHOULD match don't, or vice versa.

**The fix — integer math:**

```
int64_t a = 100000 + 200000;
// a is exactly 300000 ← which represents 0.3 correctly
// Comparing: a == 300000 → TRUE
```

Integers are exact. No precision issues ever.

### The Enums

```cpp
enum class Side : uint8_t { Buy = 0, Sell = 1 };
enum class OrderType : uint8_t { Limit = 0, Market = 1 };
```

**Limit order**: "Buy 100 shares at $150.00 or better." Has a price limit.
**Market order**: "Buy 100 shares at whatever the market offers." No price limit.

```cpp
enum class OrderStatus : uint8_t {
    New             = 0,   // Just created
    PartiallyFilled = 1,   // Some shares filled, rest waiting
    Filled          = 2,   // 100% done
    Cancelled       = 3,   // User cancelled it
    Rejected        = 4    // Invalid, never placed
};
```

Order lifecycle:
```
  New
   ├──→ PartiallyFilled ──→ Filled
   ├──→ Filled (instantly fully matched)
   ├──→ Cancelled
   └──→ Rejected (if invalid)
```

```cpp
enum class ErrorCode : uint8_t {
    Success              = 0,
    OrderNotFound        = 1,
    InvalidQuantity      = 2,
    // ... etc
};
```

We return these instead of throwing exceptions. Why? Throwing an exception can take 1000+ nanoseconds. Our cancel target is <1000ns. Returning an integer is essentially free.

### Utility Functions

```cpp
inline Price price_to_fixed(double price) {
    return static_cast<Price>(price * PRICE_MULTIPLIER);
}
```

`static_cast<Price>(...)` is a type conversion: "treat this result as a `Price` (int64_t)".

```cpp
// $100.50 → 100500000
price_to_fixed(100.50) → 100.50 * 1000000 → 100500000.0 → (int64_t) 100500000
```

```cpp
inline double price_to_double(Price price) {
    return static_cast<double>(price) / PRICE_MULTIPLIER;
}
```

The reverse: `100500000 / 1000000.0 → 100.5`. Use this only for display, never for comparisons.

---

## 4. File: `order.hpp`

**Purpose**: Define what an `Order` looks like.

### The Struct

```cpp
struct Order {
    OrderId    id              = INVALID_ORDER_ID;
    Price      price           = INVALID_PRICE;
    Quantity   quantity        = 0;
    Quantity   filled_quantity = 0;
    Side       side            = Side::Buy;
    OrderType  type            = OrderType::Limit;
    OrderStatus status         = OrderStatus::New;
    Timestamp  timestamp{};
    std::string symbol;
};
```

The `= value` after each field is a default value — what the field is set to if you don't specify otherwise.

**What each field means:**

| Field | Meaning | Example |
|-------|---------|---------|
| `id` | Unique number identifying this order | `7842` |
| `price` | Fixed-point price (limit orders only) | `150500000` = $150.50 |
| `quantity` | Total shares requested | `100` |
| `filled_quantity` | Shares actually filled so far | `30` |
| `side` | Buy or Sell | `Side::Buy` |
| `type` | Limit or Market | `OrderType::Limit` |
| `status` | Lifecycle state | `OrderStatus::New` |
| `timestamp` | When it was created | nanosecond time |
| `symbol` | What instrument | `"AAPL"` |

### The Constructor

```cpp
Order(OrderId id_, const std::string& symbol_, Side side_,
      OrderType type_, Quantity quantity_, Price price_ = INVALID_PRICE)
    : id(id_)
    , price(price_)
    , quantity(quantity_)
    , filled_quantity(0)
    , side(side_)
    , type(type_)
    , status(OrderStatus::New)
    , timestamp(now())
    , symbol(symbol_)
{}
```

The trailing `_` on parameter names (`id_`, `symbol_`) is a convention to avoid naming conflicts with member fields (`id`, `symbol`).

`Price price_ = INVALID_PRICE` means `price_` is optional — if you don't pass it, it defaults to `INVALID_PRICE`. Useful for market orders (no price needed).

### Computed Properties

```cpp
Quantity remaining_quantity() const noexcept {
    return quantity - filled_quantity;
}
```

If you ordered 100 shares and 30 have been filled, you have 70 remaining.

```cpp
bool is_filled() const noexcept {
    return filled_quantity >= quantity;
}
```

True when everything has been matched. We use `>=` instead of `==` as a safety check.

### The `fill()` Method

```cpp
Quantity fill(Quantity fill_qty) noexcept {
    Quantity actual_fill = std::min(fill_qty, remaining_quantity());

    if (actual_fill > 0) {
        filled_quantity += actual_fill;

        if (is_filled()) {
            status = OrderStatus::Filled;
        } else {
            status = OrderStatus::PartiallyFilled;
        }
    }

    return actual_fill;
}
```

`std::min(a, b)` returns whichever is smaller. We use it to avoid "overfilling" — you can't fill more than what's remaining.

Then we update `filled_quantity` and set the status accordingly.

### The `validate_order()` Function

```cpp
inline ErrorCode validate_order(const Order& order) {
    if (order.quantity == 0) return ErrorCode::InvalidQuantity;
    if (order.type == OrderType::Limit && order.price <= 0) return ErrorCode::InvalidPrice;
    if (order.symbol.empty()) return ErrorCode::BookNotFound;
    return ErrorCode::Success;
}
```

Three checks:
1. Can't order 0 shares
2. Limit orders must have a positive price (market orders don't need one)
3. Must specify an instrument

---

## 5. File: `trade.hpp`

**Purpose**: Define what a completed match looks like.

### Order vs. Trade — Critical Distinction

```
ORDER:  A request. "I want to buy 100 AAPL at $150."
TRADE:  A completed deal. "100 AAPL traded at $150 between buyer #42 and seller #99."
```

One incoming order can create multiple trades if it matches at multiple price levels.

### The Struct

```cpp
struct Trade {
    TradeId    id            = INVALID_TRADE_ID;
    OrderId    buy_order_id  = INVALID_ORDER_ID;
    OrderId    sell_order_id = INVALID_ORDER_ID;
    std::string symbol;
    Price      price         = INVALID_PRICE;
    Quantity   quantity      = 0;
    Timestamp  timestamp{};
    Side       aggressor_side = Side::Buy;
};
```

| Field | Meaning |
|-------|---------|
| `id` | Unique trade ID |
| `buy_order_id` | ID of the buy order involved |
| `sell_order_id` | ID of the sell order involved |
| `price` | Price the trade happened at (always the resting order's price) |
| `quantity` | How many shares traded |
| `aggressor_side` | Who "attacked" — the incoming order's side |

### Why the trade price is the resting order's price

```
Resting order: SELL 100 shares at $150.00 (already sitting in the book)
Incoming order: BUY  100 shares at $151.00 (just arrived)

Trade price = $150.00 (the resting order's price, not the incoming's)
```

The incoming buyer gets price improvement — they were willing to pay $151 but only paid $150.
This is standard exchange behavior.

---

## 6. File: `price_level.hpp` + `price_level.cpp`

**Purpose**: Manage all orders sitting at ONE specific price.

Think of a price level as a line (queue) of people, all offering to buy (or sell) at exactly the same price. The person who arrived first gets served first.

### The Data

```cpp
private:
    Price               price_          = INVALID_PRICE;
    Quantity            total_quantity_ = 0;
    std::list<Order*>   orders_;
```

- `price_`: What price is this level at? (e.g., `150500000` = $150.50)
- `total_quantity_`: Cached sum of all orders' remaining quantities
- `orders_`: A linked list of pointers to orders, in arrival order

**Why cache `total_quantity_`?**
Without it, computing total quantity means looping through all orders — O(n). With it, we just read a single number — O(1).

**Why `std::list<Order*>` (list of pointers, not list of orders)?**
The orders are "owned" by whoever called `add_order`. The price level just references them. Storing pointers avoids copying the order data.

### `add_order()`

```cpp
PriceLevel::OrderIterator PriceLevel::add_order(Order* order) {
    orders_.push_back(order);                         // Add to end of queue
    total_quantity_ += order->remaining_quantity();   // Update cached total
    return std::prev(orders_.end());                  // Return iterator to it
}
```

`push_back` adds to the END of the list (newest arrivals wait at the back).
`std::prev(orders_.end())` gets an iterator to the last element.

**Why return the iterator?** The caller (`OrderBook`) stores this iterator in a lookup map. Later, when this order needs to be cancelled, we use the stored iterator to remove it in O(1) — no searching required.

### `remove_order()`

```cpp
void PriceLevel::remove_order(OrderIterator it) {
    Order* order = *it;                              // Dereference iterator to get the order
    total_quantity_ -= order->remaining_quantity();  // Update cached total
    orders_.erase(it);                               // Remove from list (O(1))
}
```

`*it` dereferences the iterator — gives us the `Order*` that the iterator points to.
`orders_.erase(it)` removes the element at that iterator position. For a `std::list`, this is O(1) — just pointer updates, no shifting.

### `front()`

```cpp
Order* PriceLevel::front() noexcept {
    if (orders_.empty()) return nullptr;
    return orders_.front();
}
```

`orders_.front()` returns the first element (the oldest order — highest time priority).
Returns `nullptr` if empty, so callers must check before using.

---

## 7. File: `order_book.hpp` + `order_book.cpp`

**Purpose**: The main engine. Holds all bids and asks, runs the matching algorithm.

### `OrderLocation` — The Key to O(1) Cancel

```cpp
struct OrderLocation {
    Side                       side;
    Price                      price;
    PriceLevel::OrderIterator  iterator;  // ← this is the magic
    Order*                     order;
};
```

When an order enters the book, we record WHERE it lives:
- Which side (bids or asks)
- Which price level
- The exact iterator to its position in that price level's list
- A pointer to the order itself

With this, cancelling an order is instant — we jump directly to it.

### `OrderBook` Data Members

```cpp
private:
    std::string symbol_;

    std::map<Price, PriceLevel, std::greater<Price>> bids_;   // Highest price first
    std::map<Price, PriceLevel, std::less<Price>>    asks_;   // Lowest price first

    std::unordered_map<OrderId, OrderLocation> order_lookup_;

    TradeId next_trade_id_ = 0;
```

**`bids_`**: A sorted map from price → PriceLevel, sorted highest-price-first.
- `bids_.begin()` always gives the BEST (highest) bid.
- `std::greater<Price>` is the comparator that makes it descend.

**`asks_`**: A sorted map from price → PriceLevel, sorted lowest-price-first.
- `asks_.begin()` always gives the BEST (lowest) ask.
- `std::less<Price>` is the default comparator (ascending).

**`order_lookup_`**: A hash map from OrderId → OrderLocation.
- O(1) average lookup by order ID.
- Used for cancellation and for cleaning up during matching.

### `add_order()` — The Entry Point

```cpp
std::vector<Trade> OrderBook::add_order(Order* order) {
    std::vector<Trade> trades;

    if (validate_order(*order) != ErrorCode::Success) {
        order->status = OrderStatus::Rejected;
        return trades;   // Return empty trades vector
    }

    match_order(order, trades);

    if (order->remaining_quantity() > 0 && order->is_limit()) {
        add_to_book(order);
    }

    return trades;
}
```

Three steps:
1. **Validate** — Reject garbage orders immediately.
2. **Match** — Try to fill the incoming order against resting orders.
3. **Rest** — If a limit order has leftover quantity, add it to the book to wait.

Market orders with leftover quantity are discarded (not added to book) — that's intentional. A market order means "fill me now at any price"; if there's nothing left to match, the remaining quantity is gone.

### `match_order()` — The Heart of the Engine

```cpp
Quantity OrderBook::match_order(Order* incoming, std::vector<Trade>& trades) {
    auto& opposite_book = incoming->is_buy() ? asks_ : bids_;

    while (incoming->remaining_quantity() > 0 && !opposite_book.empty()) {
        auto level_it   = opposite_book.begin();      // Best price on opposite side
        Price resting_price = level_it->first;
        PriceLevel& level   = level_it->second;

        if (!prices_cross(incoming, resting_price)) break;

        while (incoming->remaining_quantity() > 0 && !level.empty()) {
            Order* resting = level.front();
            Quantity fill_qty = std::min(incoming->remaining_quantity(),
                                         resting->remaining_quantity());

            incoming->fill(fill_qty);
            resting->fill(fill_qty);

            trades.emplace_back(
                next_trade_id(),
                incoming->is_buy() ? incoming->id : resting->id,
                incoming->is_sell() ? incoming->id : resting->id,
                symbol_,
                resting_price,
                fill_qty,
                incoming->side
            );

            if (resting->is_filled()) {
                auto order_it = order_lookup_.find(resting->id);
                if (order_it != order_lookup_.end()) {
                    level.remove_order(order_it->second.iterator);
                    order_lookup_.erase(order_it);
                }
            }
        }

        if (level.empty()) opposite_book.erase(level_it);
    }

    return incoming->remaining_quantity();
}
```

Let's trace through it:

**Line by line:**

```cpp
auto& opposite_book = incoming->is_buy() ? asks_ : bids_;
```
If the incoming order is a BUY, match it against the ASKS (sellers). If SELL, match against BIDS (buyers).
`auto&` means "infer the type, and take a reference (not a copy)."

```cpp
while (incoming->remaining_quantity() > 0 && !opposite_book.empty()) {
```
Keep matching as long as: (a) the incoming order still wants shares, AND (b) there are resting orders to match against.

```cpp
auto level_it = opposite_book.begin();
Price resting_price = level_it->first;    // The map key (price)
PriceLevel& level   = level_it->second;  // The map value (PriceLevel)
```
Get the best price level. For bids, `begin()` = highest price. For asks, `begin()` = lowest price.
`->first` is the key (Price). `->second` is the value (PriceLevel).

```cpp
if (!prices_cross(incoming, resting_price)) break;
```
Do prices "cross"? For a BUY order: does the incoming price >= resting price? If not, stop — no more matches possible.

```cpp
while (incoming->remaining_quantity() > 0 && !level.empty()) {
    Order* resting = level.front();  // Oldest order at this price (time priority)
    Quantity fill_qty = std::min(incoming->remaining_quantity(),
                                  resting->remaining_quantity());
```
Inner loop: fill orders at this price level one by one. Fill as much as we can (min of what each side has).

```cpp
    incoming->fill(fill_qty);
    resting->fill(fill_qty);
```
Update both orders' quantities.

```cpp
    trades.emplace_back(
        next_trade_id(), ...
    );
```
`emplace_back` constructs a Trade directly in the vector (more efficient than creating one and then copying it in).

```cpp
    if (resting->is_filled()) {
        auto order_it = order_lookup_.find(resting->id);
        if (order_it != order_lookup_.end()) {
            level.remove_order(order_it->second.iterator);
            order_lookup_.erase(order_it);
        }
    }
```
If the resting order is fully filled: remove it from both the price level list AND the lookup map.
`order_it->second.iterator` is the stored iterator — gives us O(1) removal from the list.

```cpp
if (level.empty()) opposite_book.erase(level_it);
```
If that price level is now empty (no more orders), remove the price level itself from the map.

### `prices_cross()`

```cpp
static bool prices_cross(const Order* incoming, Price resting_price) noexcept {
    if (incoming->is_market()) return true;           // Market orders always cross
    if (incoming->is_buy())    return incoming->price >= resting_price;
    return                            incoming->price <= resting_price;
}
```

For a BUY limit order at $151: crosses any ask at $151 or below.
For a SELL limit order at $149: crosses any bid at $149 or above.
Market orders cross everything (that's the definition of a market order).

### `cancel_order()`

```cpp
ErrorCode OrderBook::cancel_order(OrderId order_id) {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) {
        return ErrorCode::OrderNotFound;
    }

    OrderLocation& location = it->second;
    Order* order = location.order;

    if (order->status == OrderStatus::Cancelled) return ErrorCode::OrderAlreadyCancelled;
    if (order->status == OrderStatus::Filled)    return ErrorCode::OrderAlreadyFilled;

    order->cancel();
    remove_from_book(location);
    order_lookup_.erase(it);

    return ErrorCode::Success;
}
```

Step by step:
1. Look up where the order lives in O(1) via `order_lookup_`
2. Guard against double-cancel and cancelling a filled order
3. Mark the order as Cancelled
4. Remove it from the price level
5. Remove it from the lookup map

### `best_bid()` and `best_ask()`

```cpp
std::optional<Price> OrderBook::best_bid() const noexcept {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}
```

`std::nullopt` is the "empty" optional — nothing to return.
`bids_.begin()->first` is the key (Price) of the first entry in the bids map, which is the highest price because of `std::greater<Price>`.

### `get_or_create_level()`

```cpp
PriceLevel& OrderBook::get_or_create_level(Side side, Price price) {
    auto& book = (side == Side::Buy) ? bids_ : asks_;
    PriceLevel& level = book[price];            // Creates a default PriceLevel if missing
    if (level.price() == INVALID_PRICE) {
        level = PriceLevel(price);              // Initialize it properly
    }
    return level;
}
```

`book[price]` on a `std::map` auto-creates a default-constructed entry if the key doesn't exist.
We then check if it was just created (price is still `INVALID_PRICE`) and initialize it properly.

---

## 8. End-to-End Flow Diagrams

### A. Adding a Limit Order (No Match)

```
User calls: add_order(BUY 100 @ $149.00)

    validate_order()
         │
         ▼ (passes)
    match_order()
         │
         ▼ (asks_.begin() is $150.00, incoming is $149.00)
    prices_cross? → 149.00 >= 150.00? → NO → break
         │
         ▼ (no trades)
    remaining_quantity > 0? → YES
    is_limit()? → YES
         │
         ▼
    add_to_book()
         │
    ┌────┴────────────────────────────────────────────────┐
    │  get_or_create_level(Buy, 149000000)                │
    │  level.add_order(order) → returns iterator          │
    │  order_lookup_[order->id] = {Buy, 149, it, order}  │
    └─────────────────────────────────────────────────────┘
         │
         ▼
    return [] (empty trades)
```

### B. Adding a Limit Order (Full Match)

```
Book state before:
  ASKS: $150.00 → [SELL 100 @ $150]

User calls: add_order(BUY 150 @ $151.00)

    validate_order() → passes

    match_order()
         │
         ▼
    opposite_book = asks_
    asks_.begin() → $150.00 level
    prices_cross? → 151.00 >= 150.00? → YES
         │
         ▼ (inner loop)
    resting = level.front() → SELL 100
    fill_qty = min(150, 100) = 100
    incoming->fill(100) → filled_qty=100, remaining=50
    resting->fill(100)  → filled_qty=100, remaining=0
         │
         ▼
    create Trade { BUY_ID, SELL_ID, $150, qty=100 }
         │
         ▼
    resting->is_filled()? → YES
    Remove resting from level and from order_lookup_
         │
         ▼
    level.empty()? → YES → erase $150.00 from asks_
         │
         ▼ (back to outer loop)
    asks_ is now empty → loop exits
         │
         ▼
    remaining = 50
    is_limit()? → YES → add_to_book(BUY 50 @ $151)

    return [Trade{qty=100, price=$150}]
```

### C. Cancelling an Order

```
Book state:
  BIDS: $149.00 → [BUY_A(100), BUY_B(50), BUY_C(75)]
  order_lookup_: { A: {Buy, $149, it_A, &A}, B: {Buy, $149, it_B, &B}, C: ... }

User calls: cancel_order(B)

    order_lookup_.find(B) → found! → location = {Buy, $149, it_B, &B}

    order->status checks → B is New → OK to cancel

    order->cancel() → B.status = Cancelled

    remove_from_book(location):
        book = bids_
        level = bids_[$149.00]
        level.remove_order(it_B)  ← O(1), uses stored iterator
            │
            └── list: [BUY_A] ←→ [BUY_B] ←→ [BUY_C]
                                      ↑ erase this
                result: [BUY_A] ←→ [BUY_C]

        level.empty()? → NO → don't remove the price level

    order_lookup_.erase(B)

    return ErrorCode::Success
```

### D. Overall Memory Structure

```
OrderBook "AAPL"
│
├── bids_ (std::map, sorted highest-first)
│   │
│   ├── $150.50 → PriceLevel
│   │               ├── price_ = 150500000
│   │               ├── total_quantity_ = 150
│   │               └── orders_ = [Order#1(100)] → [Order#2(50)]
│   │
│   └── $150.00 → PriceLevel
│                   ├── price_ = 150000000
│                   ├── total_quantity_ = 200
│                   └── orders_ = [Order#3(200)]
│
├── asks_ (std::map, sorted lowest-first)
│   │
│   ├── $150.75 → PriceLevel
│   │               └── orders_ = [Order#4(300)]
│   │
│   └── $151.00 → PriceLevel
│                   └── orders_ = [Order#5(100)]
│
└── order_lookup_ (std::unordered_map)
    ├── 1 → { Buy, $150.50, it→Order#1, Order#1* }
    ├── 2 → { Buy, $150.50, it→Order#2, Order#2* }
    ├── 3 → { Buy, $150.00, it→Order#3, Order#3* }
    ├── 4 → { Sell, $150.75, it→Order#4, Order#4* }
    └── 5 → { Sell, $151.00, it→Order#5, Order#5* }
```

---

## Quick Reference: Where to Find Things

| Question | Answer |
|----------|--------|
| What's a Price and why is it an integer? | `types.hpp`, Price section |
| What fields does an order have? | `order.hpp`, the `struct Order` block |
| How does fill() work? | `order.hpp`, the `fill()` method |
| How does a price level store orders? | `price_level.hpp` / `price_level.cpp` |
| How is cancel O(1)? | `order_book.hpp` `OrderLocation`, + cancel_order in `.cpp` |
| How does matching decide if prices cross? | `order_book.cpp`, `prices_cross()` |
| What is aggressor_side in a trade? | `trade.hpp`, the comment on `aggressor_side` |
| Why do we use std::optional for best_bid? | `order_book.cpp`, `best_bid()` — book might be empty |
