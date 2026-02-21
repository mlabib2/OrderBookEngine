#include <gtest/gtest.h>
#include "order_book.hpp"

using namespace orderbook;

// ============================================================================
// Test Fixture
// Provides a fresh OrderBook and helpers for creating orders.
// ============================================================================

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        book = OrderBook("AAPL");
        next_id_ = 1;
    }

    Order make_limit_buy(Quantity qty, double price) {
        return Order(next_id_++, "AAPL", Side::Buy, OrderType::Limit, qty, price_to_fixed(price));
    }

    Order make_limit_sell(Quantity qty, double price) {
        return Order(next_id_++, "AAPL", Side::Sell, OrderType::Limit, qty, price_to_fixed(price));
    }

    Order make_market_buy(Quantity qty) {
        return Order(next_id_++, "AAPL", Side::Buy, OrderType::Market, qty);
    }

    Order make_market_sell(Quantity qty) {
        return Order(next_id_++, "AAPL", Side::Sell, OrderType::Market, qty);
    }

    OrderBook book{};
    OrderId next_id_ = 1;
};

// ============================================================================
// Empty Book Queries
// ============================================================================

TEST_F(OrderBookTest, EmptyBookHasNoBestBid) {
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST_F(OrderBookTest, EmptyBookHasNoBestAsk) {
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST_F(OrderBookTest, EmptyBookHasNoSpread) {
    EXPECT_FALSE(book.spread().has_value());
}

TEST_F(OrderBookTest, EmptyBookReportsZeroOrders) {
    EXPECT_EQ(book.order_count(), 0u);
    EXPECT_TRUE(book.empty());
}

// ============================================================================
// Adding Orders That Don't Match
// ============================================================================

TEST_F(OrderBookTest, AddLimitBidNoMatchReportsCorrectBestBid) {
    auto buy = make_limit_buy(100, 150.0);
    auto trades = book.add_order(&buy);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book.order_count(), 1u);
    EXPECT_EQ(book.best_bid().value(), price_to_fixed(150.0));
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST_F(OrderBookTest, AddLimitAskNoMatchReportsCorrectBestAsk) {
    auto sell = make_limit_sell(100, 151.0);
    auto trades = book.add_order(&sell);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book.order_count(), 1u);
    EXPECT_EQ(book.best_ask().value(), price_to_fixed(151.0));
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST_F(OrderBookTest, SpreadIsCorrect) {
    auto buy  = make_limit_buy(100, 150.0);
    auto sell = make_limit_sell(100, 151.0);
    book.add_order(&buy);
    book.add_order(&sell);

    EXPECT_EQ(book.spread().value(), price_to_fixed(1.0));
}

TEST_F(OrderBookTest, BestBidIsHighestPrice) {
    auto b1 = make_limit_buy(100, 148.0);
    auto b2 = make_limit_buy(100, 150.0);
    auto b3 = make_limit_buy(100, 149.0);
    book.add_order(&b1);
    book.add_order(&b2);
    book.add_order(&b3);

    EXPECT_EQ(book.best_bid().value(), price_to_fixed(150.0));
}

TEST_F(OrderBookTest, BestAskIsLowestPrice) {
    auto s1 = make_limit_sell(100, 152.0);
    auto s2 = make_limit_sell(100, 150.5);
    auto s3 = make_limit_sell(100, 151.0);
    book.add_order(&s1);
    book.add_order(&s2);
    book.add_order(&s3);

    EXPECT_EQ(book.best_ask().value(), price_to_fixed(150.5));
}

TEST_F(OrderBookTest, VolumeAtPrice) {
    auto b1 = make_limit_buy(100, 150.0);
    auto b2 = make_limit_buy(50,  150.0);
    book.add_order(&b1);
    book.add_order(&b2);

    EXPECT_EQ(book.volume_at_price(Side::Buy, price_to_fixed(150.0)), 150u);
    EXPECT_EQ(book.volume_at_price(Side::Buy, price_to_fixed(149.0)), 0u);
}

// ============================================================================
// Matching: Limit Orders
// ============================================================================

TEST_F(OrderBookTest, ExactMatchClearsBook) {
    auto sell = make_limit_sell(100, 150.0);
    auto buy  = make_limit_buy(100, 150.0);
    book.add_order(&sell);
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_EQ(trades[0].price, price_to_fixed(150.0));
    EXPECT_TRUE(book.empty());
}

TEST_F(OrderBookTest, IncomingBuyMatchesRestingSellAtRestingPrice) {
    auto sell = make_limit_sell(100, 150.0);
    book.add_order(&sell);

    auto buy = make_limit_buy(100, 151.0);  // Willing to pay more
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price, price_to_fixed(150.0));  // Resting order's price
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_TRUE(book.empty());
}

TEST_F(OrderBookTest, IncomingSellMatchesRestingBuyAtRestingPrice) {
    auto buy = make_limit_buy(100, 151.0);
    book.add_order(&buy);

    auto sell = make_limit_sell(100, 150.0);
    auto trades = book.add_order(&sell);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price, price_to_fixed(151.0));  // Resting order's price
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_TRUE(book.empty());
}

TEST_F(OrderBookTest, NoCrossNoCrossNoMatch) {
    auto sell = make_limit_sell(100, 151.0);
    book.add_order(&sell);

    auto buy = make_limit_buy(100, 150.0);  // Below ask — no match
    auto trades = book.add_order(&buy);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(book.order_count(), 2u);
}

// ============================================================================
// Matching: Partial Fills
// ============================================================================

TEST_F(OrderBookTest, IncomingBuyPartiallyFilledRestsWithRemainder) {
    auto sell = make_limit_sell(60, 150.0);
    book.add_order(&sell);

    auto buy = make_limit_buy(100, 150.0);
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 60u);

    // Buy has 40 left and rests on the bid side
    EXPECT_EQ(book.order_count(), 1u);
    EXPECT_EQ(book.best_bid().value(), price_to_fixed(150.0));
    EXPECT_FALSE(book.best_ask().has_value());
    EXPECT_EQ(buy.remaining_quantity(), 40u);
    EXPECT_EQ(buy.status, OrderStatus::PartiallyFilled);
}

TEST_F(OrderBookTest, IncomingSellPartiallyFilledRestsWithRemainder) {
    auto buy = make_limit_buy(60, 150.0);
    book.add_order(&buy);

    auto sell = make_limit_sell(100, 150.0);
    auto trades = book.add_order(&sell);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 60u);
    EXPECT_EQ(sell.remaining_quantity(), 40u);
    EXPECT_EQ(sell.status, OrderStatus::PartiallyFilled);
}

TEST_F(OrderBookTest, RestingOrderPartiallyFilledStaysOnBook) {
    auto sell = make_limit_sell(100, 150.0);
    book.add_order(&sell);

    auto buy = make_limit_buy(40, 150.0);
    book.add_order(&buy);

    // Sell still has 60 left, should remain in the ask book
    EXPECT_EQ(book.order_count(), 1u);
    EXPECT_EQ(book.best_ask().value(), price_to_fixed(150.0));
    EXPECT_EQ(book.volume_at_price(Side::Sell, price_to_fixed(150.0)), 60u);
    EXPECT_EQ(sell.status, OrderStatus::PartiallyFilled);
}

// ============================================================================
// Matching: Multi-Level Sweep
// ============================================================================

TEST_F(OrderBookTest, BuySweepsMultipleAskLevels) {
    auto s1 = make_limit_sell(50, 150.0);
    auto s2 = make_limit_sell(50, 151.0);
    auto s3 = make_limit_sell(50, 152.0);
    book.add_order(&s1);
    book.add_order(&s2);
    book.add_order(&s3);

    // Buy willing to pay up to $152, needs 120 shares
    auto buy = make_limit_buy(120, 152.0);
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 3u);
    EXPECT_EQ(trades[0].price, price_to_fixed(150.0));  // Best ask first
    EXPECT_EQ(trades[0].quantity, 50u);
    EXPECT_EQ(trades[1].price, price_to_fixed(151.0));
    EXPECT_EQ(trades[1].quantity, 50u);
    EXPECT_EQ(trades[2].price, price_to_fixed(152.0));
    EXPECT_EQ(trades[2].quantity, 20u);  // Only 20 of the 50 needed

    // 30 shares remain at $152 ask
    EXPECT_EQ(book.volume_at_price(Side::Sell, price_to_fixed(152.0)), 30u);
}

TEST_F(OrderBookTest, SellSweepsMultipleBidLevels) {
    auto b1 = make_limit_buy(50, 152.0);
    auto b2 = make_limit_buy(50, 151.0);
    auto b3 = make_limit_buy(50, 150.0);
    book.add_order(&b1);
    book.add_order(&b2);
    book.add_order(&b3);

    auto sell = make_limit_sell(120, 150.0);
    auto trades = book.add_order(&sell);

    ASSERT_EQ(trades.size(), 3u);
    EXPECT_EQ(trades[0].price, price_to_fixed(152.0));  // Best bid first
    EXPECT_EQ(trades[1].price, price_to_fixed(151.0));
    EXPECT_EQ(trades[2].price, price_to_fixed(150.0));
    EXPECT_EQ(trades[2].quantity, 20u);

    EXPECT_EQ(book.volume_at_price(Side::Buy, price_to_fixed(150.0)), 30u);
}

TEST_F(OrderBookTest, EmptiedPriceLevelsAreRemovedFromBook) {
    auto s1 = make_limit_sell(50, 150.0);
    auto s2 = make_limit_sell(50, 151.0);
    book.add_order(&s1);
    book.add_order(&s2);

    auto buy = make_limit_buy(100, 151.0);
    book.add_order(&buy);

    EXPECT_EQ(book.ask_levels(), 0u);  // Both levels consumed
}

// ============================================================================
// Matching: FIFO Priority
// ============================================================================

TEST_F(OrderBookTest, FIFOPriorityEarlierOrderMatchesFirst) {
    auto s1 = make_limit_sell(50, 150.0);  // Arrives first
    auto s2 = make_limit_sell(50, 150.0);  // Arrives second
    book.add_order(&s1);
    book.add_order(&s2);

    auto buy = make_limit_buy(50, 150.0);
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].sell_order_id, s1.id);  // s1 matched, not s2
    EXPECT_EQ(s1.status, OrderStatus::Filled);
    EXPECT_EQ(s2.status, OrderStatus::New);      // s2 still resting
}

TEST_F(OrderBookTest, FIFOPrioritySecondOrderMatchesAfterFirst) {
    auto s1 = make_limit_sell(50, 150.0);
    auto s2 = make_limit_sell(50, 150.0);
    book.add_order(&s1);
    book.add_order(&s2);

    auto buy = make_limit_buy(100, 150.0);
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].sell_order_id, s1.id);
    EXPECT_EQ(trades[1].sell_order_id, s2.id);
}

// ============================================================================
// Matching: Market Orders
// ============================================================================

TEST_F(OrderBookTest, MarketBuyMatchesRestingSells) {
    auto sell = make_limit_sell(100, 150.0);
    book.add_order(&sell);

    auto buy = make_market_buy(100);
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_TRUE(book.empty());
}

TEST_F(OrderBookTest, MarketSellMatchesRestingBuys) {
    auto buy = make_limit_buy(100, 150.0);
    book.add_order(&buy);

    auto sell = make_market_sell(100);
    auto trades = book.add_order(&sell);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_TRUE(book.empty());
}

TEST_F(OrderBookTest, MarketOrderDoesNotRestOnBookWhenUnfilled) {
    // Nothing on the opposite side — market order remainder is discarded
    auto buy = make_market_buy(100);
    auto trades = book.add_order(&buy);

    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(book.empty());  // NOT added to book
}

TEST_F(OrderBookTest, MarketOrderCrossesAnyPrice) {
    auto s1 = make_limit_sell(50, 200.0);  // Very high ask
    auto s2 = make_limit_sell(50, 500.0);  // Even higher
    book.add_order(&s1);
    book.add_order(&s2);

    auto buy = make_market_buy(100);
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].price, price_to_fixed(200.0));
    EXPECT_EQ(trades[1].price, price_to_fixed(500.0));
}

// ============================================================================
// Validation
// ============================================================================

TEST_F(OrderBookTest, InvalidOrderIsRejectedImmediately) {
    auto bad = Order(next_id_++, "AAPL", Side::Buy, OrderType::Limit, 0, price_to_fixed(150.0));
    auto trades = book.add_order(&bad);

    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(bad.status, OrderStatus::Rejected);
    EXPECT_TRUE(book.empty());
}

// ============================================================================
// cancel_order()
// ============================================================================

TEST_F(OrderBookTest, CancelBidSuccess) {
    auto buy = make_limit_buy(100, 150.0);
    book.add_order(&buy);

    EXPECT_EQ(book.cancel_order(buy.id), ErrorCode::Success);
    EXPECT_EQ(buy.status, OrderStatus::Cancelled);
    EXPECT_EQ(book.order_count(), 0u);
    EXPECT_FALSE(book.best_bid().has_value());
}

TEST_F(OrderBookTest, CancelAskSuccess) {
    auto sell = make_limit_sell(100, 151.0);
    book.add_order(&sell);

    EXPECT_EQ(book.cancel_order(sell.id), ErrorCode::Success);
    EXPECT_EQ(sell.status, OrderStatus::Cancelled);
    EXPECT_FALSE(book.best_ask().has_value());
}

TEST_F(OrderBookTest, CancelNonExistentOrderReturnsNotFound) {
    EXPECT_EQ(book.cancel_order(9999), ErrorCode::OrderNotFound);
}

TEST_F(OrderBookTest, CancelFilledOrderReturnsNotFound) {
    auto sell = make_limit_sell(100, 150.0);
    auto buy  = make_limit_buy(100, 150.0);
    book.add_order(&sell);
    book.add_order(&buy);  // sell is fully matched and removed

    // sell was removed from order_lookup_ after being filled
    EXPECT_EQ(book.cancel_order(sell.id), ErrorCode::OrderNotFound);
}

TEST_F(OrderBookTest, CancelAfterCancelReturnsNotFound) {
    auto buy = make_limit_buy(100, 150.0);
    book.add_order(&buy);
    book.cancel_order(buy.id);

    // Second cancel: removed from lookup on first cancel
    EXPECT_EQ(book.cancel_order(buy.id), ErrorCode::OrderNotFound);
}

TEST_F(OrderBookTest, CancelRemovesEmptyPriceLevel) {
    auto buy = make_limit_buy(100, 150.0);
    book.add_order(&buy);
    book.cancel_order(buy.id);

    EXPECT_EQ(book.bid_levels(), 0u);
}

TEST_F(OrderBookTest, CancelOneOfManyLeavesOthersIntact) {
    auto b1 = make_limit_buy(100, 150.0);
    auto b2 = make_limit_buy(50,  150.0);
    book.add_order(&b1);
    book.add_order(&b2);

    book.cancel_order(b1.id);

    EXPECT_EQ(book.order_count(), 1u);
    EXPECT_EQ(book.bid_levels(), 1u);  // Level still exists (b2 is there)
    EXPECT_EQ(book.volume_at_price(Side::Buy, price_to_fixed(150.0)), 50u);
}

TEST_F(OrderBookTest, CancelPartiallyFilledOrderSuccess) {
    auto sell = make_limit_sell(100, 150.0);
    book.add_order(&sell);

    auto buy = make_limit_buy(40, 150.0);
    book.add_order(&buy);  // Partially fills sell (60 remaining)

    EXPECT_EQ(book.cancel_order(sell.id), ErrorCode::Success);
    EXPECT_EQ(sell.status, OrderStatus::Cancelled);
    EXPECT_FALSE(book.best_ask().has_value());
}

// ============================================================================
// Trade Fields
// ============================================================================

TEST_F(OrderBookTest, TradeAggressorSideIsBuyForIncomingBuy) {
    auto sell = make_limit_sell(100, 150.0);
    book.add_order(&sell);

    auto buy = make_limit_buy(100, 150.0);
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].aggressor_side, Side::Buy);
    EXPECT_EQ(trades[0].buy_order_id, buy.id);
    EXPECT_EQ(trades[0].sell_order_id, sell.id);
}

TEST_F(OrderBookTest, TradeAggressorSideIsSellForIncomingSell) {
    auto buy = make_limit_buy(100, 150.0);
    book.add_order(&buy);

    auto sell = make_limit_sell(100, 150.0);
    auto trades = book.add_order(&sell);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].aggressor_side, Side::Sell);
    EXPECT_EQ(trades[0].buy_order_id, buy.id);
    EXPECT_EQ(trades[0].sell_order_id, sell.id);
}

TEST_F(OrderBookTest, TradeSymbolMatchesBook) {
    auto sell = make_limit_sell(100, 150.0);
    book.add_order(&sell);

    auto buy = make_limit_buy(100, 150.0);
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].symbol, "AAPL");
}

TEST_F(OrderBookTest, TradeIdsAreUnique) {
    auto s1 = make_limit_sell(50, 150.0);
    auto s2 = make_limit_sell(50, 151.0);
    book.add_order(&s1);
    book.add_order(&s2);

    auto buy = make_limit_buy(100, 151.0);
    auto trades = book.add_order(&buy);

    ASSERT_EQ(trades.size(), 2u);
    EXPECT_NE(trades[0].id, trades[1].id);
}
