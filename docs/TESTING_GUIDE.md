# Testing Guide: GoogleTest from Scratch

This guide teaches you how to read, write, and run unit tests for this project.
No prior testing experience assumed.

---

## Table of Contents

1. [What is a Unit Test?](#1-what-is-a-unit-test)
2. [GoogleTest Syntax Explained](#2-googletest-syntax-explained)
3. [The Three Kinds of Tests We Write](#3-the-three-kinds-of-tests-we-write)
4. [How to Run Tests](#4-how-to-run-tests)
5. [How to Write a New Test](#5-how-to-write-a-new-test)
6. [Reading Test Output](#6-reading-test-output)
7. [What Makes a Good Test](#7-what-makes-a-good-test)

---

## 1. What is a Unit Test?

A unit test is a small program that:
1. Sets up a specific situation ("given a filled order...")
2. Calls one function ("...when we try to cancel it...")
3. Checks the result ("...it should return false")

The goal: if you break something in the code, a test fails and tells you exactly what broke.

Without tests, you'd have to manually run the engine and check everything by hand after every change. With 80 tests, you know in 7 seconds whether everything still works.

---

## 2. GoogleTest Syntax Explained

### Basic Test

```cpp
TEST(GroupName, TestName) {
    // ... code to run ...
    EXPECT_EQ(actual_value, expected_value);
}
```

- `TEST` is a macro (a keyword that expands into a lot of hidden code)
- `GroupName` is the category — usually the class being tested
- `TestName` is what this specific test checks
- Inside the braces: run some code, then assert the result

**Example from our tests:**

```cpp
TEST(OrderTest, FullFill) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    Quantity filled = o.fill(100);

    EXPECT_EQ(filled, 100u);
    EXPECT_EQ(o.status, OrderStatus::Filled);
    EXPECT_TRUE(o.is_filled());
}
```

Reading this like English:
> "Test that when an Order is filled completely, the return value is 100,
>  the status becomes Filled, and is_filled() returns true."

---

### Assertion Macros

These are the lines that actually CHECK something. If the check fails, the test fails.

| Macro | What it checks | Example |
|-------|---------------|---------|
| `EXPECT_EQ(a, b)` | a equals b | `EXPECT_EQ(qty, 100u)` |
| `EXPECT_NE(a, b)` | a does NOT equal b | `EXPECT_NE(id1, id2)` |
| `EXPECT_TRUE(x)` | x is true | `EXPECT_TRUE(order.is_filled())` |
| `EXPECT_FALSE(x)` | x is false | `EXPECT_FALSE(book.empty())` |
| `EXPECT_LT(a, b)` | a < b | `EXPECT_LT(latency, 1000)` |
| `EXPECT_GT(a, b)` | a > b | `EXPECT_GT(qty, 0u)` |
| `ASSERT_EQ(a, b)` | a equals b — STOP if it fails | `ASSERT_EQ(trades.size(), 1u)` |

**`EXPECT` vs `ASSERT`:**

```cpp
// EXPECT: test continues even if this fails
EXPECT_EQ(trades.size(), 1u);
EXPECT_EQ(trades[0].price, price_to_fixed(150.0));  // runs even if above fails

// ASSERT: test STOPS immediately if this fails
ASSERT_EQ(trades.size(), 1u);
EXPECT_EQ(trades[0].price, price_to_fixed(150.0));  // only runs if above passed
```

Use `ASSERT` when the next line would crash if the assertion failed.
For example: if `trades` is empty and you check `trades[0]`, that's undefined behavior.
So always `ASSERT_EQ(trades.size(), 1u)` before accessing `trades[0]`.

---

### Test Fixtures (The `_F` Tests)

A fixture is a class that holds shared setup code, so you don't repeat it in every test.

```cpp
// 1. Define the fixture class
class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {       // Runs BEFORE each test
        book = OrderBook("AAPL");
        next_id_ = 1;
    }
    // void TearDown() override {}  // Runs AFTER each test (optional)

    OrderBook book{};
    OrderId next_id_ = 1;
};

// 2. Use TEST_F instead of TEST, pass the fixture class name
TEST_F(OrderBookTest, EmptyBookHasNoBestBid) {
    // `book` is already created and ready — SetUp() ran first
    EXPECT_FALSE(book.best_bid().has_value());
}
```

**Why fixtures?**

Without a fixture, every test would need:

```cpp
TEST(OrderBookTest, EmptyBookHasNoBestBid) {
    OrderBook book("AAPL");  // repeated in every single test
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST(OrderBookTest, AddOrder) {
    OrderBook book("AAPL");  // copy-pasted again
    // ...
}
```

With a fixture, `SetUp()` runs automatically. Each test gets a fresh `book` object — they don't share state.

---

### The `u` Suffix on Numbers

```cpp
EXPECT_EQ(qty, 100u);   // the 'u' means: this number is unsigned
```

`Quantity` is `uint64_t` (unsigned). If you compare it to a plain `100`, the compiler warns about a signed/unsigned comparison. Writing `100u` makes the literal unsigned too. Same type, no warning.

---

### `has_value()` for Optionals

```cpp
std::optional<Price> best = book.best_bid();

if (best.has_value()) {       // Is there a value?
    Price p = best.value();   // Get it
    Price p2 = *best;         // Same thing, shorter syntax
}

EXPECT_TRUE(best.has_value());               // assert it's not empty
EXPECT_EQ(best.value(), price_to_fixed(150.0)); // assert the value itself
```

---

## 3. The Three Kinds of Tests We Write

### Kind 1: Unit Tests for a Struct/Class

Tests one class in isolation. No other classes involved.

```cpp
// Testing just the Order struct
TEST(OrderTest, PartialFill) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    Quantity filled = o.fill(30);

    EXPECT_EQ(filled, 30u);
    EXPECT_EQ(o.filled_quantity, 30u);
    EXPECT_EQ(o.remaining_quantity(), 70u);
    EXPECT_EQ(o.status, OrderStatus::PartiallyFilled);
}
```

Pattern: create → act → assert.

---

### Kind 2: Behavior Tests

Tests that a specific rule/behavior is enforced.

```cpp
// Rule: fill() should never fill more than remaining quantity
TEST(OrderTest, FillDoesNotExceedRemaining) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    Quantity filled = o.fill(200);  // Try to fill 200, but only 100 available

    EXPECT_EQ(filled, 100u);        // Should cap at 100
    EXPECT_EQ(o.filled_quantity, 100u);
}
```

---

### Kind 3: Integration Tests (via Fixture)

Tests that multiple classes work together correctly.

```cpp
// Tests Order + PriceLevel + OrderBook all working together
TEST_F(OrderBookTest, FIFOPriorityEarlierOrderMatchesFirst) {
    auto s1 = make_limit_sell(50, 150.0);  // Arrives first
    auto s2 = make_limit_sell(50, 150.0);  // Arrives second
    book.add_order(&s1);
    book.add_order(&s2);

    auto buy = make_limit_buy(50, 150.0);
    auto trades = book.add_order(&buy);

    // FIFO rule: s1 arrived first, so s1 must match first
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].sell_order_id, s1.id);
    EXPECT_EQ(s1.status, OrderStatus::Filled);
    EXPECT_EQ(s2.status, OrderStatus::New);   // s2 untouched
}
```

---

## 4. How to Run Tests

### Step 1: Build (first time only, or after code changes)

```bash
cd cpp

# Create the build directory
mkdir -p build && cd build

# Configure (only needed once, or after CMakeLists.txt changes)
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_BENCHMARKS=OFF \
      -DCMAKE_OSX_SYSROOT=$(xcrun --show-sdk-path) ..

# Build
cmake --build . -j$(sysctl -n hw.logicalcpu)
```

### Step 2: Run all tests

```bash
# From inside cpp/build/:
ctest --output-on-failure
```

### Run a specific test group

```bash
./orderbook_tests --gtest_filter="OrderTest.*"         # All Order tests
./orderbook_tests --gtest_filter="PriceLevelTest.*"    # All PriceLevel tests
./orderbook_tests --gtest_filter="OrderBookTest.*"     # All OrderBook tests
```

### Run a specific single test

```bash
./orderbook_tests --gtest_filter="OrderTest.PartialFill"
./orderbook_tests --gtest_filter="OrderBookTest.FIFOPriorityEarlierOrderMatchesFirst"
```

### Run with verbose output (see all test names even when passing)

```bash
./orderbook_tests --gtest_filter="*" -v
```

### After changing code — rebuild and rerun

```bash
# From cpp/build/:
cmake --build . && ctest --output-on-failure
```

---

## 5. How to Write a New Test

Say you want to test: "If I add two buy orders at different prices, the one with the higher price is best bid."

### Step 1: Pick the right file

| What you're testing | File |
|---------------------|------|
| `Order` struct or `validate_order()` | `test_order.cpp` |
| `PriceLevel` class | `test_price_level.cpp` |
| `OrderBook` matching/cancel/queries | `test_order_book.cpp` |

### Step 2: Write the test

```cpp
// In test_order_book.cpp

TEST_F(OrderBookTest, HigherPricedBidIsBestBid) {
    // ARRANGE: set up the situation
    auto low  = make_limit_buy(100, 149.0);
    auto high = make_limit_buy(100, 150.0);

    book.add_order(&low);
    book.add_order(&high);

    // ACT: do the thing you're testing
    auto best = book.best_bid();

    // ASSERT: check the result
    ASSERT_TRUE(best.has_value());
    EXPECT_EQ(best.value(), price_to_fixed(150.0));
}
```

**The pattern is always: Arrange → Act → Assert**

### Step 3: Build and run

```bash
cmake --build . && ./orderbook_tests --gtest_filter="OrderBookTest.HigherPricedBidIsBestBid"
```

### Step 4: Watch it pass (or fail and fix it)

---

### Adding a test for a bug you found

Say you discovered: "When the same order ID is added twice, strange things happen."

Write the test BEFORE you fix the bug:

```cpp
TEST_F(OrderBookTest, DuplicateOrderIdHandledSafely) {
    auto buy = make_limit_buy(100, 150.0);
    book.add_order(&buy);

    // Try to add the same order object again
    auto trades = book.add_order(&buy);

    // The book should not have two entries for the same order
    EXPECT_EQ(book.order_count(), 1u);
}
```

Run it → watch it fail → fix the bug → run again → watch it pass. This is called Test-Driven Development (TDD).

---

## 6. Reading Test Output

### All passing

```
[==========] Running 80 tests from 3 test suites.
[----------] 28 tests from OrderTest
[ RUN      ] OrderTest.DefaultConstruction
[       OK ] OrderTest.DefaultConstruction (0 ms)
...
[  PASSED  ] 80 tests.
```

### One failure

```
[ RUN      ] OrderBookTest.ExactMatchClearsBook
/path/to/test_order_book.cpp:52: Failure
Expected equality of these values:
  trades.size()
    Which is: 0           ← what you GOT
  1u
    Which is: 1           ← what you EXPECTED
[  FAILED  ] OrderBookTest.ExactMatchClearsBook (0 ms)
```

Reading a failure:
1. The test name: `OrderBookTest.ExactMatchClearsBook`
2. The file and line: `test_order_book.cpp:52` — go here to see the `EXPECT` that failed
3. What you got vs. what you expected

### Summary at the end

```
[  PASSED  ] 79 tests.
[  FAILED  ] 1 test, listed below:
[  FAILED  ] OrderBookTest.ExactMatchClearsBook
```

---

## 7. What Makes a Good Test

### DO: Test one thing per test

```cpp
// BAD: tests too many things, hard to know what failed
TEST_F(OrderBookTest, LotsOfStuff) {
    auto buy = make_limit_buy(100, 150.0);
    auto sell = make_limit_sell(100, 150.0);
    book.add_order(&buy);
    book.add_order(&sell);
    EXPECT_TRUE(book.empty());
    EXPECT_EQ(buy.status, OrderStatus::Filled);
    EXPECT_EQ(sell.status, OrderStatus::Filled);
    EXPECT_EQ(book.best_bid().has_value(), false);
    // ... 10 more asserts
}

// GOOD: each test has one clear purpose
TEST_F(OrderBookTest, ExactMatchClearsBook) {
    auto buy = make_limit_buy(100, 150.0);
    auto sell = make_limit_sell(100, 150.0);
    book.add_order(&sell);
    auto trades = book.add_order(&buy);
    EXPECT_TRUE(book.empty());
}

TEST_F(OrderBookTest, ExactMatchSetsStatusToFilled) {
    auto buy = make_limit_buy(100, 150.0);
    auto sell = make_limit_sell(100, 150.0);
    book.add_order(&sell);
    book.add_order(&buy);
    EXPECT_EQ(buy.status, OrderStatus::Filled);
    EXPECT_EQ(sell.status, OrderStatus::Filled);
}
```

### DO: Test edge cases

The most important tests are often the boundary conditions:

```cpp
// Edge cases to always consider:
// - Empty state (no orders, no matches)
// - Exactly 1 item
// - The minimum valid value
// - The maximum valid value
// - Invalid input (zero qty, empty symbol, etc.)
// - What happens when you do the same operation twice
```

### DO: Name tests like a sentence

```
CancelAfterCancelReturnsNotFound
FIFOPriorityEarlierOrderMatchesFirst
MarketOrderDoesNotRestOnBookWhenUnfilled
```

Read it: "cancel after cancel returns not found." If this test fails, you know exactly what the problem is.

### DON'T: Test implementation details

```cpp
// BAD: tests internal state that users shouldn't care about
TEST_F(OrderBookTest, InternalMapHasTwoEntries) {
    book.add_order(&b1);
    book.add_order(&b2);
    EXPECT_EQ(book.bids_.size(), 2u);  // bids_ is private! Don't test this.
}

// GOOD: test behavior through the public API
TEST_F(OrderBookTest, TwoOrdersAtDifferentPricesCreateTwoBidLevels) {
    book.add_order(&b1);
    book.add_order(&b2);
    EXPECT_EQ(book.bid_levels(), 2u);  // bid_levels() is public
}
```
