#include "price_level.hpp"

namespace orderbook {

// ============================================================================
// Constructors
// ============================================================================

PriceLevel::PriceLevel(Price price)
    : price_(price)
    , total_quantity_(0)
    , orders_()
{}

// ============================================================================
// Order Management
// ============================================================================

PriceLevel::OrderIterator PriceLevel::add_order(Order* order) {
    // Add to back of queue (FIFO - newest orders match last)
    orders_.push_back(order);

    // Update cached total quantity
    total_quantity_ += order->remaining_quantity();

    // Return iterator to the newly added order
    // This is the last element, so we use std::prev(end())
    return std::prev(orders_.end());
}

void PriceLevel::remove_order(OrderIterator it) {
    Order* order = *it;
    total_quantity_ -= order->remaining_quantity();
    orders_.erase(it);
}

void PriceLevel::reduce_quantity(Quantity qty) noexcept {
    total_quantity_ -= qty;
}

// ============================================================================
// Accessors
// ============================================================================

Order* PriceLevel::front() noexcept {
    if (orders_.empty()) {
        return nullptr;
    }
    return orders_.front();
}

const Order* PriceLevel::front() const noexcept {
    if (orders_.empty()) {
        return nullptr;
    }
    return orders_.front();
}

} // namespace orderbook
