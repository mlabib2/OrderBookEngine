#ifndef ORDERBOOK_PRICE_LEVEL_HPP
#define ORDERBOOK_PRICE_LEVEL_HPP

#include "types.hpp"
#include "order.hpp"
#include <list>

namespace orderbook {

// ============================================================================
// PriceLevel Class
// ============================================================================
//
// Manages all orders at a single price point.
//
// WHY std::list?
//   - FIFO ordering: First order in should be first to match (time priority)
//   - O(1) push_back: Adding new orders is constant time
//   - O(1) erase with iterator: Cancelling any order is constant time
//   - No iterator invalidation: Other iterators stay valid when we remove
//
// WHY NOT std::vector?
//   - Removing from front is O(n) - have to shift all elements
//   - Removing from middle invalidates iterators
//
// WHY NOT std::deque?
//   - Removing from middle is still O(n)
//   - We need to cancel orders that aren't at the front
//
// VISUAL:
//   PriceLevel at $100.50:
//   [Order A (100 shares)] -> [Order B (50 shares)] -> [Order C (200 shares)]
//        ^
//        First to match (time priority)
//

class PriceLevel {
public:
    // Iterator type for external access (used by OrderBook for O(1) cancel)
    using OrderIterator = std::list<Order*>::iterator;
    using ConstOrderIterator = std::list<Order*>::const_iterator;

    // ========================================================================
    // Constructors
    // ========================================================================

    // Create a price level at the given price
    explicit PriceLevel(Price price);

    // Default constructor (for map default construction)
    PriceLevel() = default;

    // ========================================================================
    // Order Management
    // ========================================================================

    // Add an order to the back of the queue (FIFO)
    // Returns iterator to the added order (store this for O(1) cancel later)
    OrderIterator add_order(Order* order);

    // Remove an order using its iterator (O(1) operation)
    // The iterator must be valid and point to an order in this level
    void remove_order(OrderIterator it);

    // ========================================================================
    // Accessors
    // ========================================================================

    // Get the price of this level
    Price price() const noexcept { return price_; }

    // Get total quantity across all orders at this level
    Quantity total_quantity() const noexcept { return total_quantity_; }

    // Get the number of orders at this level
    size_t order_count() const noexcept { return orders_.size(); }

    // Is this level empty?
    bool empty() const noexcept { return orders_.empty(); }

    // Get the first order (front of FIFO queue) - for matching
    // Returns nullptr if empty
    Order* front() noexcept;
    const Order* front() const noexcept;

    // ========================================================================
    // Iteration (for matching through orders)
    // ========================================================================

    OrderIterator begin() noexcept { return orders_.begin(); }
    OrderIterator end() noexcept { return orders_.end(); }
    ConstOrderIterator begin() const noexcept { return orders_.begin(); }
    ConstOrderIterator end() const noexcept { return orders_.end(); }

private:
    // The price for this level (all orders here have this price)
    Price price_ = INVALID_PRICE;

    // Total quantity of all orders (cached for O(1) access)
    // Invariant: total_quantity_ == sum of remaining_quantity() for all orders
    Quantity total_quantity_ = 0;

    // Orders in FIFO order (front = oldest = first to match)
    std::list<Order*> orders_;
};

} // namespace orderbook

#endif // ORDERBOOK_PRICE_LEVEL_HPP
