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

// ============================================================================
// OrderLocation Structure
// ============================================================================
//
// Stores the exact location of an order in the book for O(1) cancel.
//
// THE ITERATOR TRICK:
//   When we add an order to a PriceLevel's std::list, we get back an iterator.
//   We store that iterator here. Later, when cancel is called, we can use
//   this iterator to erase from the list in O(1) - no searching needed!
//
// WITHOUT this trick:
//   Cancel would be O(n): scan all orders at the price level to find ours.
//
// WITH this trick:
//   Cancel is O(1): hash lookup + iterator erase.
//

struct OrderLocation {
    Side side = Side::Buy;
    Price price = INVALID_PRICE;
    PriceLevel::OrderIterator iterator;  // Direct pointer into the list
    Order* order = nullptr;              // Direct pointer to order data
};

// ============================================================================
// OrderBook Class
// ============================================================================
//
// Manages all orders for a single instrument and executes the matching algorithm.
//
// MATCHING ALGORITHM: Price-Time Priority
//   1. Price Priority: Better prices match first
//      - For incoming BUY: lowest ASK matches first
//      - For incoming SELL: highest BID matches first
//   2. Time Priority: At the same price, older orders match first (FIFO)
//
// DATA STRUCTURES:
//   bids_: std::map<Price, PriceLevel, std::greater<Price>>
//          Sorted descending, so begin() = best (highest) bid
//
//   asks_: std::map<Price, PriceLevel, std::less<Price>>
//          Sorted ascending, so begin() = best (lowest) ask
//
//   order_lookup_: std::unordered_map<OrderId, OrderLocation>
//          O(1) lookup for cancel operations
//
// COMPLEXITY:
//   add_order (no match): O(log n) - map insertion
//   add_order (k matches): O(log n + k) - map + k list operations
//   cancel_order: O(1) amortized - hash lookup + iterator erase
//   best_bid/ask: O(1) - map.begin()
//
// MEMORY OWNERSHIP:
//   OrderBook does NOT own the Order objects. Caller owns the memory.
//   OrderBook stores pointers and iterators to track order locations.
//   When an order is cancelled or filled, it's removed from the book,
//   but the Order object itself is not deleted.
//

class OrderBook {
public:
    // ========================================================================
    // Constructors
    // ========================================================================

    // Create an order book for the given symbol
    explicit OrderBook(const std::string& symbol);

    // Default constructor (empty symbol)
    OrderBook() = default;

    // ========================================================================
    // Core Operations
    // ========================================================================

    // Add an order to the book
    // Matches against resting orders if prices cross
    // Returns list of trades generated (may be empty if no match)
    //
    // FLOW:
    //   1. Validate order
    //   2. Match against opposite side
    //   3. If remaining quantity and limit order: add to book
    //   4. Return trades
    //
    // MARKET ORDERS:
    //   - Match as much as possible at available prices
    //   - Never rest on the book (unfilled portion is rejected)
    //
    std::vector<Trade> add_order(Order* order);

    // Cancel an order by ID
    // Returns Success if cancelled, error code otherwise
    //
    // POSSIBLE ERRORS:
    //   - OrderNotFound: No order with this ID in the book
    //   - OrderAlreadyCancelled: Order was already cancelled
    //   - OrderAlreadyFilled: Order was already fully filled
    //
    ErrorCode cancel_order(OrderId order_id);

    // ========================================================================
    // Market Data Accessors
    // ========================================================================

    // Get the best (highest) bid price
    // Returns nullopt if no bids
    std::optional<Price> best_bid() const noexcept;

    // Get the best (lowest) ask price
    // Returns nullopt if no asks
    std::optional<Price> best_ask() const noexcept;

    // Get the spread (best_ask - best_bid)
    // Returns nullopt if either side is empty
    std::optional<Price> spread() const noexcept;

    // Get total quantity at a specific price level
    // Returns 0 if no orders at that price
    Quantity volume_at_price(Side side, Price price) const noexcept;

    // ========================================================================
    // Book State Accessors
    // ========================================================================

    // Get the symbol this book is for
    const std::string& symbol() const noexcept { return symbol_; }

    // Get total number of active orders in the book
    size_t order_count() const noexcept { return order_lookup_.size(); }

    // Is the book empty (no orders)?
    bool empty() const noexcept { return order_lookup_.empty(); }

    // Get number of price levels on each side
    size_t bid_levels() const noexcept { return bids_.size(); }
    size_t ask_levels() const noexcept { return asks_.size(); }

private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================

    // Match an incoming order against the opposite side of the book
    // Fills the trades vector with any matches
    // Returns remaining quantity after matching
    Quantity match_order(Order* order, std::vector<Trade>& trades);

    // Add an order to the book (no matching, just placement)
    void add_to_book(Order* order);

    // Remove an order from the book using its location
    void remove_from_book(const OrderLocation& location);

    // Get or create a price level at the given price
    // For bids, uses bids_ map; for asks, uses asks_ map
    PriceLevel& get_or_create_level(Side side, Price price);

    // Generate a new unique trade ID
    TradeId next_trade_id() noexcept { return ++next_trade_id_; }

    // Check if prices cross (incoming can match against resting)
    // For BUY: incoming.price >= resting.price (willing to pay at least ask)
    // For SELL: incoming.price <= resting.price (willing to accept at least bid)
    static bool prices_cross(const Order* incoming, Price resting_price) noexcept;

    // ========================================================================
    // Member Variables
    // ========================================================================

    // Symbol this book is for (e.g., "AAPL", "BTCUSDT")
    std::string symbol_;

    // Bid side: sorted descending (highest price = best = begin())
    // std::greater<Price> makes the map sort in descending order
    std::map<Price, PriceLevel, std::greater<Price>> bids_;

    // Ask side: sorted ascending (lowest price = best = begin())
    // std::less<Price> is the default, but we're explicit for clarity
    std::map<Price, PriceLevel, std::less<Price>> asks_;

    // Fast lookup for cancel operations
    // Maps OrderId -> location in the book
    std::unordered_map<OrderId, OrderLocation> order_lookup_;

    // Counter for generating unique trade IDs
    TradeId next_trade_id_ = 0;
};

} // namespace orderbook

#endif // ORDERBOOK_ORDER_BOOK_HPP
