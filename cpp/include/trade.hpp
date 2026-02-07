#ifndef ORDERBOOK_TRADE_HPP
#define ORDERBOOK_TRADE_HPP

#include "types.hpp"
#include <string>

namespace orderbook {

// ============================================================================
// Trade Structure
// ============================================================================
//
// Represents a completed match between a buy order and a sell order.
//
// A Trade is created when:
//   1. An incoming order crosses the spread (matches a resting order)
//   2. Both orders have their filled_quantity updated
//   3. This Trade record captures the execution details
//
// IMPORTANT DISTINCTION:
//   - Order: A request to buy/sell (may or may not execute)
//   - Trade: A completed transaction (always executed)
//
// One incoming order can generate multiple trades if it matches
// against several resting orders at different price levels.
//

struct Trade {
    // Unique identifier for this trade
    TradeId id = INVALID_TRADE_ID;

    // The two orders involved
    // buy_order_id is always the buyer, sell_order_id is always the seller
    OrderId buy_order_id = INVALID_ORDER_ID;
    OrderId sell_order_id = INVALID_ORDER_ID;

    // Instrument that was traded
    std::string symbol;

    // Execution price (always the resting order's price)
    // WHY resting order's price?
    //   The resting order was there first and "set" the price.
    //   The aggressor "takes" that price.
    //   This is standard exchange behavior (price improvement for aggressor).
    Price price = INVALID_PRICE;

    // Amount traded
    Quantity quantity = 0;

    // When the trade occurred
    Timestamp timestamp{};

    // Who initiated this trade (the incoming/aggressor order)
    // If aggressor_side == Buy: a buy order came in and hit a resting sell
    // If aggressor_side == Sell: a sell order came in and hit a resting buy
    Side aggressor_side = Side::Buy;

    // ========================================================================
    // Constructors
    // ========================================================================

    // Default constructor
    Trade() = default;

    // Full constructor
    Trade(TradeId id_,
          OrderId buy_order_id_,
          OrderId sell_order_id_,
          const std::string& symbol_,
          Price price_,
          Quantity quantity_,
          Side aggressor_side_)
        : 
        id(id_)
        , buy_order_id(buy_order_id_)
        , sell_order_id(sell_order_id_)
        , symbol(symbol_)
        , price(price_)
        , quantity(quantity_)
        , timestamp(now())
        , aggressor_side(aggressor_side_)
    {}

    // ========================================================================
    // Computed Properties
    // ========================================================================

    // Get the value of this trade in price units
    // Note: This is price * quantity, still in fixed-point
    // To get dollar value: trade_value() / PRICE_MULTIPLIER
    int64_t trade_value() const noexcept {
        return price * static_cast<int64_t>(quantity);
    }

    // Get the aggressor's order ID
    OrderId aggressor_order_id() const noexcept {
        return aggressor_side == Side::Buy ? buy_order_id : sell_order_id;
    }

    // Get the passive (resting) order ID
    OrderId passive_order_id() const noexcept {
        return aggressor_side == Side::Buy ? sell_order_id : buy_order_id;
    }
};

} // namespace orderbook

#endif // ORDERBOOK_TRADE_HPP
