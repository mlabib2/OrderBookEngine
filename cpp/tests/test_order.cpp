#include <gtest/gtest.h>
#include "order.hpp"

using namespace orderbook;

// ============================================================================
// Order Construction
// ============================================================================

TEST(OrderTest, DefaultConstruction) {
    Order o;
    EXPECT_EQ(o.id, INVALID_ORDER_ID);
    EXPECT_EQ(o.price, INVALID_PRICE);
    EXPECT_EQ(o.quantity, 0u);
    EXPECT_EQ(o.filled_quantity, 0u);
    EXPECT_EQ(o.status, OrderStatus::New);
}

TEST(OrderTest, FullConstruction) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    EXPECT_EQ(o.id, 1u);
    EXPECT_EQ(o.symbol, "AAPL");
    EXPECT_EQ(o.side, Side::Buy);
    EXPECT_EQ(o.type, OrderType::Limit);
    EXPECT_EQ(o.quantity, 100u);
    EXPECT_EQ(o.price, price_to_fixed(150.0));
    EXPECT_EQ(o.filled_quantity, 0u);
    EXPECT_EQ(o.status, OrderStatus::New);
}

// ============================================================================
// Computed Properties
// ============================================================================

TEST(OrderTest, RemainingQuantityInitial) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    EXPECT_EQ(o.remaining_quantity(), 100u);
}

TEST(OrderTest, RemainingQuantityAfterPartialFill) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    o.filled_quantity = 30;
    EXPECT_EQ(o.remaining_quantity(), 70u);
}

TEST(OrderTest, IsFilledFalseInitially) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    EXPECT_FALSE(o.is_filled());
}

TEST(OrderTest, IsFilledTrueWhenComplete) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    o.filled_quantity = 100;
    EXPECT_TRUE(o.is_filled());
}

TEST(OrderTest, IsActiveForNewOrder) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    EXPECT_TRUE(o.is_active());
}

TEST(OrderTest, IsActiveForPartiallyFilledOrder) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    o.status = OrderStatus::PartiallyFilled;
    EXPECT_TRUE(o.is_active());
}

TEST(OrderTest, IsActiveReturnsFalseWhenFilled) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    o.status = OrderStatus::Filled;
    EXPECT_FALSE(o.is_active());
}

TEST(OrderTest, IsActiveReturnsFalseWhenCancelled) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    o.status = OrderStatus::Cancelled;
    EXPECT_FALSE(o.is_active());
}

TEST(OrderTest, SideHelpers) {
    Order buy(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    EXPECT_TRUE(buy.is_buy());
    EXPECT_FALSE(buy.is_sell());

    Order sell(2, "AAPL", Side::Sell, OrderType::Limit, 100, price_to_fixed(150.0));
    EXPECT_TRUE(sell.is_sell());
    EXPECT_FALSE(sell.is_buy());
}

TEST(OrderTest, TypeHelpers) {
    Order limit(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    EXPECT_TRUE(limit.is_limit());
    EXPECT_FALSE(limit.is_market());

    Order market(2, "AAPL", Side::Buy, OrderType::Market, 100);
    EXPECT_TRUE(market.is_market());
    EXPECT_FALSE(market.is_limit());
}

// ============================================================================
// fill()
// ============================================================================

TEST(OrderTest, PartialFill) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    Quantity filled = o.fill(30);

    EXPECT_EQ(filled, 30u);
    EXPECT_EQ(o.filled_quantity, 30u);
    EXPECT_EQ(o.remaining_quantity(), 70u);
    EXPECT_EQ(o.status, OrderStatus::PartiallyFilled);
    EXPECT_FALSE(o.is_filled());
    EXPECT_TRUE(o.is_active());
}

TEST(OrderTest, FullFill) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    Quantity filled = o.fill(100);

    EXPECT_EQ(filled, 100u);
    EXPECT_EQ(o.filled_quantity, 100u);
    EXPECT_EQ(o.remaining_quantity(), 0u);
    EXPECT_EQ(o.status, OrderStatus::Filled);
    EXPECT_TRUE(o.is_filled());
    EXPECT_FALSE(o.is_active());
}

TEST(OrderTest, FillDoesNotExceedRemaining) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    Quantity filled = o.fill(200);  // Try to overfill

    EXPECT_EQ(filled, 100u);        // Only 100 available
    EXPECT_EQ(o.filled_quantity, 100u);
    EXPECT_EQ(o.status, OrderStatus::Filled);
}

TEST(OrderTest, FillZeroDoesNothing) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    Quantity filled = o.fill(0);

    EXPECT_EQ(filled, 0u);
    EXPECT_EQ(o.filled_quantity, 0u);
    EXPECT_EQ(o.status, OrderStatus::New);
}

TEST(OrderTest, MultipleFillsAccumulate) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    o.fill(30);
    o.fill(40);

    EXPECT_EQ(o.filled_quantity, 70u);
    EXPECT_EQ(o.remaining_quantity(), 30u);
    EXPECT_EQ(o.status, OrderStatus::PartiallyFilled);
}

// ============================================================================
// cancel()
// ============================================================================

TEST(OrderTest, CancelNewOrder) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    bool result = o.cancel();

    EXPECT_TRUE(result);
    EXPECT_EQ(o.status, OrderStatus::Cancelled);
    EXPECT_FALSE(o.is_active());
}

TEST(OrderTest, CancelPartiallyFilledOrder) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    o.fill(30);
    bool result = o.cancel();

    EXPECT_TRUE(result);
    EXPECT_EQ(o.status, OrderStatus::Cancelled);
}

TEST(OrderTest, CancelFilledOrderFails) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    o.fill(100);
    bool result = o.cancel();

    EXPECT_FALSE(result);
    EXPECT_EQ(o.status, OrderStatus::Filled);  // Unchanged
}

TEST(OrderTest, CancelAlreadyCancelledFails) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    o.cancel();
    bool result = o.cancel();

    EXPECT_FALSE(result);
    EXPECT_EQ(o.status, OrderStatus::Cancelled);
}

// ============================================================================
// validate_order()
// ============================================================================

TEST(ValidateOrderTest, ValidLimitOrder) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    EXPECT_EQ(validate_order(o), ErrorCode::Success);
}

TEST(ValidateOrderTest, ValidMarketOrder) {
    Order o(1, "AAPL", Side::Buy, OrderType::Market, 100);
    EXPECT_EQ(validate_order(o), ErrorCode::Success);
}

TEST(ValidateOrderTest, ZeroQuantityRejected) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 0, price_to_fixed(150.0));
    EXPECT_EQ(validate_order(o), ErrorCode::InvalidQuantity);
}

TEST(ValidateOrderTest, LimitOrderWithNoPriceRejected) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, 0);
    EXPECT_EQ(validate_order(o), ErrorCode::InvalidPrice);
}

TEST(ValidateOrderTest, LimitOrderWithNegativePriceRejected) {
    Order o(1, "AAPL", Side::Buy, OrderType::Limit, 100, -1);
    EXPECT_EQ(validate_order(o), ErrorCode::InvalidPrice);
}

TEST(ValidateOrderTest, EmptySymbolRejected) {
    Order o(1, "", Side::Buy, OrderType::Limit, 100, price_to_fixed(150.0));
    EXPECT_EQ(validate_order(o), ErrorCode::BookNotFound);
}

TEST(ValidateOrderTest, MarketOrderDoesNotNeedPrice) {
    // Market order with price = 0 is still valid
    Order o(1, "AAPL", Side::Buy, OrderType::Market, 50);
    EXPECT_EQ(validate_order(o), ErrorCode::Success);
}
