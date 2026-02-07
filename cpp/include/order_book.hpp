#ifndef ORDERBOOK_ORDER_BOOK_HPP
#define ORDERBOOK_ORDER_BOOK_HPP

#include "types.hpp"
#include "order.hpp"
#include "trade.hpp"
#include "price_level.hpp"
#include <map>
#include <unordered_map>
#include <vector>
#include <optional>
#include <string>

namespace orderbook {

// Tracks where an order lives in the book for O(1) cancel.
// The iterator lets us erase from std::list without searching.
struct OrderLocation {
    Side side = Side::Buy;
    Price price = INVALID_PRICE;
    PriceLevel::OrderIterator iterator;
    Order* order = nullptr;
};

// Order book for a single instrument. Matches orders using price-time priority.
//
// Complexity: add O(log n), cancel O(1), best_bid/ask O(1)
class OrderBook {
public:
    explicit OrderBook(const std::string& symbol);
    OrderBook() = default;

    // Match incoming order against resting orders, return generated trades
    std::vector<Trade> add_order(Order* order);
    ErrorCode cancel_order(OrderId order_id);

    std::optional<Price> best_bid() const noexcept;
    std::optional<Price> best_ask() const noexcept;
    std::optional<Price> spread() const noexcept;
    Quantity volume_at_price(Side side, Price price) const noexcept;

    const std::string& symbol() const noexcept { return symbol_; }
    size_t order_count() const noexcept { return order_lookup_.size(); }
    bool empty() const noexcept { return order_lookup_.empty(); }
    size_t bid_levels() const noexcept { return bids_.size(); }
    size_t ask_levels() const noexcept { return asks_.size(); }

private:
    Quantity match_order(Order* order, std::vector<Trade>& trades);
    void add_to_book(Order* order);
    void remove_from_book(const OrderLocation& location);
    PriceLevel& get_or_create_level(Side side, Price price);
    TradeId next_trade_id() noexcept { return ++next_trade_id_; }
    static bool prices_cross(const Order* incoming, Price resting_price) noexcept;

    std::string symbol_;
    std::map<Price, PriceLevel, std::greater<Price>> bids_;  // Highest first
    std::map<Price, PriceLevel, std::less<Price>> asks_;     // Lowest first
    std::unordered_map<OrderId, OrderLocation> order_lookup_;
    TradeId next_trade_id_ = 0;
};

} // namespace orderbook

#endif // ORDERBOOK_ORDER_BOOK_HPP
