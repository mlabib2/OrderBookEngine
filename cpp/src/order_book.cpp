#include "order_book.hpp"
#include <algorithm>

namespace orderbook {

OrderBook::OrderBook(const std::string& symbol)
    : symbol_(symbol)
{}

std::vector<Trade> OrderBook::add_order(Order* order) {
    std::vector<Trade> trades;

    if (validate_order(*order) != ErrorCode::Success) {
        order->status = OrderStatus::Rejected;
        return trades;
    }

    match_order(order, trades);

    // Limit orders with remaining qty rest on the book
    if (order->remaining_quantity() > 0 && order->is_limit()) {
        add_to_book(order);
    }

    return trades;
}

ErrorCode OrderBook::cancel_order(OrderId order_id) {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) {
        return ErrorCode::OrderNotFound;
    }

    OrderLocation& location = it->second;
    Order* order = location.order;

    if (order->status == OrderStatus::Cancelled) {
        return ErrorCode::OrderAlreadyCancelled;
    }
    if (order->status == OrderStatus::Filled) {
        return ErrorCode::OrderAlreadyFilled;
    }

    order->cancel();
    remove_from_book(location);
    order_lookup_.erase(it);

    return ErrorCode::Success;
}

std::optional<Price> OrderBook::best_bid() const noexcept {
    if (bids_.empty()) return std::nullopt;
    return bids_.begin()->first;
}

std::optional<Price> OrderBook::best_ask() const noexcept {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

std::optional<Price> OrderBook::spread() const noexcept {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) return std::nullopt;
    return *ask - *bid;
}

Quantity OrderBook::volume_at_price(Side side, Price price) const noexcept {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        return (it != bids_.end()) ? it->second.total_quantity() : 0;
    } else {
        auto it = asks_.find(price);
        return (it != asks_.end()) ? it->second.total_quantity() : 0;
    }
}

Quantity OrderBook::match_order(Order* incoming, std::vector<Trade>& trades) {
    // bids_ and asks_ have different comparator types so we can't use a ternary.
    // A generic lambda lets us write the matching logic once and call it with either map.
    auto do_match = [&](auto& opposite_book) {
        while (incoming->remaining_quantity() > 0 && !opposite_book.empty()) {
            auto level_it = opposite_book.begin();
            Price resting_price = level_it->first;
            PriceLevel& level = level_it->second;

            if (!prices_cross(incoming, resting_price)) {
                break;
            }

            while (incoming->remaining_quantity() > 0 && !level.empty()) {
                Order* resting = level.front();
                Quantity fill_qty = std::min(incoming->remaining_quantity(),
                                             resting->remaining_quantity());

                incoming->fill(fill_qty);
                resting->fill(fill_qty);
                level.reduce_quantity(fill_qty);

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

            if (level.empty()) {
                opposite_book.erase(level_it);
            }
        }
    };

    if (incoming->is_buy()) {
        do_match(asks_);
    } else {
        do_match(bids_);
    }

    return incoming->remaining_quantity();
}

void OrderBook::add_to_book(Order* order) {
    PriceLevel& level = get_or_create_level(order->side, order->price);
    auto it = level.add_order(order);

    order_lookup_[order->id] = OrderLocation{
        order->side,
        order->price,
        it,
        order
    };
}

void OrderBook::remove_from_book(const OrderLocation& location) {
    auto do_remove = [&](auto& book) {
        auto level_it = book.find(location.price);
        if (level_it == book.end()) return;
        PriceLevel& level = level_it->second;
        level.remove_order(location.iterator);
        if (level.empty()) {
            book.erase(level_it);
        }
    };

    if (location.side == Side::Buy) {
        do_remove(bids_);
    } else {
        do_remove(asks_);
    }
}

PriceLevel& OrderBook::get_or_create_level(Side side, Price price) {
    auto do_get = [&](auto& book) -> PriceLevel& {
        PriceLevel& level = book[price];
        if (level.price() == INVALID_PRICE) {
            level = PriceLevel(price);
        }
        return level;
    };

    if (side == Side::Buy) {
        return do_get(bids_);
    } else {
        return do_get(asks_);
    }
}

bool OrderBook::prices_cross(const Order* incoming, Price resting_price) noexcept {
    if (incoming->is_market()) return true;
    if (incoming->is_buy()) return incoming->price >= resting_price;
    return incoming->price <= resting_price;
}

} // namespace orderbook
