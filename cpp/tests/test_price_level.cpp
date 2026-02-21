#include <gtest/gtest.h>
#include "price_level.hpp"

using namespace orderbook;

// ============================================================================
// Test Fixture
// Sets up a PriceLevel and three orders before each test.
// ============================================================================

class PriceLevelTest : public ::testing::Test {
protected:
    void SetUp() override {
        level = PriceLevel(price_to_fixed(150.0));
        o1 = Order(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
        o2 = Order(2, "AAPL", Side::Buy, OrderType::Limit, 50,  price_to_fixed(150.0));
        o3 = Order(3, "AAPL", Side::Buy, OrderType::Limit, 75,  price_to_fixed(150.0));
    }

    PriceLevel level{};
    Order o1{}, o2{}, o3{};
};

// ============================================================================
// Initial State
// ============================================================================

TEST_F(PriceLevelTest, InitialPrice) {
    EXPECT_EQ(level.price(), price_to_fixed(150.0));
}

TEST_F(PriceLevelTest, InitiallyEmpty) {
    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.order_count(), 0u);
    EXPECT_EQ(level.total_quantity(), 0u);
    EXPECT_EQ(level.front(), nullptr);
}

// ============================================================================
// add_order()
// ============================================================================

TEST_F(PriceLevelTest, AddSingleOrder) {
    level.add_order(&o1);

    EXPECT_FALSE(level.empty());
    EXPECT_EQ(level.order_count(), 1u);
    EXPECT_EQ(level.total_quantity(), 100u);
    EXPECT_EQ(level.front(), &o1);
}

TEST_F(PriceLevelTest, AddMultipleOrders) {
    level.add_order(&o1);
    level.add_order(&o2);
    level.add_order(&o3);

    EXPECT_EQ(level.order_count(), 3u);
    EXPECT_EQ(level.total_quantity(), 225u);  // 100 + 50 + 75
}

TEST_F(PriceLevelTest, AddOrderFIFOFrontIsOldest) {
    level.add_order(&o1);  // Arrives first
    level.add_order(&o2);
    level.add_order(&o3);

    EXPECT_EQ(level.front(), &o1);  // o1 arrived first, must be at front
}

TEST_F(PriceLevelTest, AddOrderReturnsValidIterator) {
    auto it = level.add_order(&o1);
    EXPECT_EQ(*it, &o1);
}

// ============================================================================
// remove_order()
// ============================================================================

TEST_F(PriceLevelTest, RemoveFront) {
    auto it1 = level.add_order(&o1);
    level.add_order(&o2);

    level.remove_order(it1);

    EXPECT_EQ(level.order_count(), 1u);
    EXPECT_EQ(level.total_quantity(), 50u);
    EXPECT_EQ(level.front(), &o2);  // o2 is now first
}

TEST_F(PriceLevelTest, RemoveMiddle) {
    level.add_order(&o1);
    auto it2 = level.add_order(&o2);
    level.add_order(&o3);

    level.remove_order(it2);

    EXPECT_EQ(level.order_count(), 2u);
    EXPECT_EQ(level.total_quantity(), 175u);  // 100 + 75
    EXPECT_EQ(level.front(), &o1);            // o1 still first
}

TEST_F(PriceLevelTest, RemoveLast) {
    level.add_order(&o1);
    level.add_order(&o2);
    auto it3 = level.add_order(&o3);

    level.remove_order(it3);

    EXPECT_EQ(level.order_count(), 2u);
    EXPECT_EQ(level.total_quantity(), 150u);  // 100 + 50
}

TEST_F(PriceLevelTest, RemoveOnlyOrder) {
    auto it = level.add_order(&o1);
    level.remove_order(it);

    EXPECT_TRUE(level.empty());
    EXPECT_EQ(level.order_count(), 0u);
    EXPECT_EQ(level.total_quantity(), 0u);
    EXPECT_EQ(level.front(), nullptr);
}

TEST_F(PriceLevelTest, RemoveDeductsRemainingNotOriginalQuantity) {
    // o1 has qty=100, added to level: total_quantity_ = 100
    level.add_order(&o1);

    // Simulate the fill that the matching loop would normally pair with reduce_quantity.
    // Here we call reduce_quantity explicitly to keep total_quantity_ in sync.
    level.reduce_quantity(30);
    o1.fill(30);  // remaining = 70, total_quantity_ = 70

    auto it = level.begin();
    level.remove_order(it);  // Subtracts remaining (70), not original (100)

    EXPECT_EQ(level.total_quantity(), 0u);
}

// ============================================================================
// total_quantity() integrity
// ============================================================================

TEST_F(PriceLevelTest, TotalQuantityAfterMultipleAddsAndRemoves) {
    auto it1 = level.add_order(&o1);  // +100 → total=100
    level.add_order(&o2);             // +50  → total=150
    auto it3 = level.add_order(&o3); // +75  → total=225

    level.remove_order(it1);  // -100 → total=125
    level.remove_order(it3);  // -75  → total=50

    EXPECT_EQ(level.total_quantity(), 50u);
    EXPECT_EQ(level.order_count(), 1u);
}

// ============================================================================
// Iteration
// ============================================================================

TEST_F(PriceLevelTest, IterationOrderIsFIFO) {
    level.add_order(&o1);
    level.add_order(&o2);
    level.add_order(&o3);

    auto it = level.begin();
    EXPECT_EQ(*it, &o1); ++it;
    EXPECT_EQ(*it, &o2); ++it;
    EXPECT_EQ(*it, &o3); ++it;
    EXPECT_EQ(it, level.end());
}
