#include "order_book.hpp"
#include <algorithm>

namespace orderbook {

// ============================================================================
// Constructor
// ============================================================================

OrderBook::OrderBook(const std::string& symbol)
    : symbol_(symbol)
    , bids_()
    , asks_()
    , order_lookup_()
    , next_trade_id_(0)
{}

// ============================================================================
// Core Operations
// ============================================================================

std::vector<Trade> OrderBook::add_order(Order* order) {
    std::vector<Trade> trades;

    // Validate the order first
    ErrorCode validation = validate_order(*order);
    if (validation != ErrorCode::Success) {
        order->status = OrderStatus::Rejected;
        return trades;  // Empty, no trades
    }

    // Match against opposite side
    Quantity remaining = match_order(order, trades);

    // If there's remaining quantity and it's a limit order, add to book
    if (remaining > 0 && order->is_limit()) {
        add_to_book(order);
    }
    // Market orders with remaining quantity are left unfilled (not added to book)
    // The order status is already set by match_order or stays New/PartiallyFilled

    return trades;
}

ErrorCode OrderBook::cancel_order(OrderId order_id) {
    // Find the order in our lookup map
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) {
        return ErrorCode::OrderNotFound;
    }

    OrderLocation& location = it->second;
    Order* order = location.order;

    // Check if order can be cancelled
    if (order->status == OrderStatus::Cancelled) {
        return ErrorCode::OrderAlreadyCancelled;
    }
    if (order->status == OrderStatus::Filled) {
        return ErrorCode::OrderAlreadyFilled;
    }

    // Cancel the order
    order->cancel();

    // Remove from the book
    remove_from_book(location);

    // Remove from lookup
    order_lookup_.erase(it);

    return ErrorCode::Success;
}

// ============================================================================
// Market Data Accessors
// ============================================================================

std::optional<Price> OrderBook::best_bid() const noexcept {
    if (bids_.empty()) {
        return std::nullopt;
    }
    // begin() gives highest price due to std::greater comparator
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const noexcept {
    if (asks_.empty()) {
        return std::nullopt;
    }
    // begin() gives lowest price due to std::less comparator
    return asks_.begin()->first;
}

std::optional<Price> OrderBook::spread() const noexcept {
    auto bid = best_bid();
    auto ask = best_ask();

    if (!bid || !ask) {
        return std::nullopt;
    }

    return *ask - *bid;
}

Quantity OrderBook::volume_at_price(Side side, Price price) const noexcept {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it == bids_.end()) {
            return 0;
        }
        return it->second.total_quantity();
    } else {
        auto it = asks_.find(price);
        if (it == asks_.end()) {
            return 0;
        }
        return it->second.total_quantity();
    }
}

// ============================================================================
// Internal Helpers
// ============================================================================

Quantity OrderBook::match_order(Order* incoming, std::vector<Trade>& trades) {
    // Determine which side to match against
    // BUY orders match against ASKs, SELL orders match against BIDs
    auto& opposite_book = incoming->is_buy() ? asks_ : bids_;

    while (incoming->remaining_quantity() > 0 && !opposite_book.empty()) {
        // Get the best price level on the opposite side
        auto level_it = opposite_book.begin();
        Price resting_price = level_it->first;
        PriceLevel& level = level_it->second;

        // Check if prices cross
        if (!prices_cross(incoming, resting_price)) {
            break;  // No more matches possible
        }

        // Match against orders at this price level (FIFO)
        while (incoming->remaining_quantity() > 0 && !level.empty()) {
            Order* resting = level.front();

            // Calculate fill quantity
            Quantity fill_qty = std::min(
                incoming->remaining_quantity(),
                resting->remaining_quantity()
            );

            // Execute the fill on both orders
            incoming->fill(fill_qty);
            resting->fill(fill_qty);

            // Create trade record
            // Price is always the resting order's price (price improvement for aggressor)
            Trade trade(
                next_trade_id(),
                incoming->is_buy() ? incoming->id : resting->id,   // buy_order_id
                incoming->is_sell() ? incoming->id : resting->id,  // sell_order_id
                symbol_,
                resting_price,
                fill_qty,
                incoming->side  // aggressor_side
            );
            trades.push_back(trade);

            // If resting order is fully filled, remove it from the book
            if (resting->is_filled()) {
                // Get the iterator before removing
                auto order_it = order_lookup_.find(resting->id);
                if (order_it != order_lookup_.end()) {
                    OrderLocation& loc = order_it->second;
                    level.remove_order(loc.iterator);
                    order_lookup_.erase(order_it);
                }
            }
        }

        // If the price level is now empty, remove it from the map
        if (level.empty()) {
            opposite_book.erase(level_it);
        }
    }

    return incoming->remaining_quantity();
}

void OrderBook::add_to_book(Order* order) {
    // Get or create the price level
    PriceLevel& level = get_or_create_level(order->side, order->price);

    // Add order to the level, get iterator for O(1) cancel later
    PriceLevel::OrderIterator it = level.add_order(order);

    // Store location in lookup map
    OrderLocation location;
    location.side = order->side;
    location.price = order->price;
    location.iterator = it;
    location.order = order;

    order_lookup_[order->id] = location;
}

void OrderBook::remove_from_book(const OrderLocation& location) {
    // Get the correct book
    auto& book = (location.side == Side::Buy) ? bids_ : asks_;

    // Find the price level
    auto level_it = book.find(location.price);
    if (level_it == book.end()) {
        return;  // Level doesn't exist (shouldn't happen)
    }

    PriceLevel& level = level_it->second;

    // Remove the order from the level using the stored iterator
    level.remove_order(location.iterator);

    // If level is now empty, remove it from the map
    if (level.empty()) {
        book.erase(level_it);
    }
}

PriceLevel& OrderBook::get_or_create_level(Side side, Price price) {
    if (side == Side::Buy) {
        // operator[] creates a default PriceLevel if not exists
        PriceLevel& level = bids_[price];
        // If newly created, set the price (default constructor leaves it invalid)
        if (level.price() == INVALID_PRICE) {
            level = PriceLevel(price);
        }
        return level;
    } else {
        PriceLevel& level = asks_[price];
        if (level.price() == INVALID_PRICE) {
            level = PriceLevel(price);
        }
        return level;
    }
}

bool OrderBook::prices_cross(const Order* incoming, Price resting_price) noexcept {
    if (incoming->is_market()) {
        // Market orders always cross (willing to pay any price)
        return true;
    }

    if (incoming->is_buy()) {
        // BUY: willing to pay at least resting_price?
        // incoming.price >= resting_price (ask)
        return incoming->price >= resting_price;
    } else {
        // SELL: willing to accept at least resting_price?
        // incoming.price <= resting_price (bid)
        return incoming->price <= resting_price;
    }
}

} // namespace orderbook
