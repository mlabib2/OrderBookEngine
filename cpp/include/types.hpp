#ifndef ORDERBOOK_TYPES_HPP
#define ORDERBOOK_TYPES_HPP

#include <cstdint>
#include <chrono>
#include <string>

namespace orderbook {

// ============================================================================
// Type Aliases
// ============================================================================

// Order and Trade identifiers
// We use uint64_t because:
// - 64 bits = 18 quintillion unique IDs (won't run out)
// - Unsigned because IDs are never negative
// - Fixed size for predictable memory layout
using OrderId = uint64_t;
using TradeId = uint64_t;

// Price is stored as a fixed-point integer
// WHY NOT double?
//   double has precision issues: 0.1 + 0.2 != 0.3 in floating point!
//   This causes bugs when comparing prices for matching.
//
// HOW IT WORKS:
//   We multiply the real price by 1,000,000 (6 decimal places)
//   $100.50 becomes 100500000
//   $0.000001 becomes 1 (smallest representable price)
//
// RANGE:
//   int64_t max = 9,223,372,036,854,775,807
//   Divided by 1,000,000 = max price of ~9.2 quadrillion
//   More than enough for any financial instrument
using Price = int64_t;

// Quantity of shares/contracts
// uint64_t because quantities are never negative
using Quantity = uint64_t;

// Timestamp with nanosecond precision
// steady_clock is monotonic (never goes backwards) - important for ordering
using Timestamp = std::chrono::steady_clock::time_point;

// ============================================================================
// Constants
// ============================================================================

// Price conversion multiplier (6 decimal places)
// constexpr means this is computed at compile time - zero runtime cost
constexpr int64_t PRICE_MULTIPLIER = 1'000'000;

// Invalid/null values for optional fields
constexpr OrderId INVALID_ORDER_ID = 0;
constexpr TradeId INVALID_TRADE_ID = 0;
constexpr Price INVALID_PRICE = 0;

// ============================================================================
// Enums
// ============================================================================

// Order side: Buy or Sell
// Using uint8_t as underlying type to save memory (1 byte instead of 4)
enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

// Order type
// Limit: Execute at specified price or better
// Market: Execute immediately at best available price
enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1
};

// Order status (lifecycle states)
//
// State diagram:
//   New -> PartiallyFilled -> Filled
//   New -> Filled (if fully matched immediately)
//   New -> Cancelled
//   PartiallyFilled -> Cancelled
//   New -> Rejected (if invalid)
//
enum class OrderStatus : uint8_t {
    New = 0,              // Just created, not yet processed
    PartiallyFilled = 1,  // Some quantity executed, rest on book
    Filled = 2,           // Fully executed
    Cancelled = 3,        // Removed before full execution
    Rejected = 4          // Invalid order, never placed on book
};

// Error codes for operations
// WHY NOT exceptions?
//   Throwing an exception can take 1000+ nanoseconds
//   Our target is <1000ns for cancel operations
//   Error codes are just integer comparisons - essentially free
enum class ErrorCode : uint8_t {
    Success = 0,
    OrderNotFound = 1,
    InvalidQuantity = 2,
    InvalidPrice = 3,
    InvalidSide = 4,
    InvalidOrderType = 5,
    BookNotFound = 6,
    InsufficientLiquidity = 7,  // Market order can't be fully filled
    OrderAlreadyCancelled = 8,
    OrderAlreadyFilled = 9
};

// ============================================================================
// Price Conversion Utilities
// ============================================================================

// Convert a double price to fixed-point
// Example: price_to_fixed(100.50) returns 100500000
inline Price price_to_fixed(double price) {
    return static_cast<Price>(price * PRICE_MULTIPLIER);
}

// Convert fixed-point back to double (for display only!)
// Example: price_to_double(100500000) returns 100.50
inline double price_to_double(Price price) {
    return static_cast<double>(price) / PRICE_MULTIPLIER;
}

// ============================================================================
// Utility Functions
// ============================================================================

// Get current timestamp
inline Timestamp now() {
    return std::chrono::steady_clock::now();
}

// Convert timestamp to nanoseconds since epoch (for logging/serialization)
inline int64_t timestamp_to_nanos(Timestamp ts) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        ts.time_since_epoch()
    ).count();
}

// String conversions for debugging/logging
inline const char* to_string(Side side) {
    return side == Side::Buy ? "BUY" : "SELL";
}

inline const char* to_string(OrderType type) {
    switch (type) {
        case OrderType::Limit:  return "LIMIT";
        case OrderType::Market: return "MARKET";
        default:                return "UNKNOWN";
    }
}

inline const char* to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::New:             return "NEW";
        case OrderStatus::PartiallyFilled: return "PARTIALLY_FILLED";
        case OrderStatus::Filled:          return "FILLED";
        case OrderStatus::Cancelled:       return "CANCELLED";
        case OrderStatus::Rejected:        return "REJECTED";
        default:                           return "UNKNOWN";
    }
}

inline const char* to_string(ErrorCode error) {
    switch (error) {
        case ErrorCode::Success:              return "SUCCESS";
        case ErrorCode::OrderNotFound:        return "ORDER_NOT_FOUND";
        case ErrorCode::InvalidQuantity:      return "INVALID_QUANTITY";
        case ErrorCode::InvalidPrice:         return "INVALID_PRICE";
        case ErrorCode::InvalidSide:          return "INVALID_SIDE";
        case ErrorCode::InvalidOrderType:     return "INVALID_ORDER_TYPE";
        case ErrorCode::BookNotFound:         return "BOOK_NOT_FOUND";
        case ErrorCode::InsufficientLiquidity: return "INSUFFICIENT_LIQUIDITY";
        case ErrorCode::OrderAlreadyCancelled: return "ORDER_ALREADY_CANCELLED";
        case ErrorCode::OrderAlreadyFilled:   return "ORDER_ALREADY_FILLED";
        default:                              return "UNKNOWN_ERROR";
    }
}

// Get the opposite side (useful for matching)
inline Side opposite_side(Side side) {
    return side == Side::Buy ? Side::Sell : Side::Buy;
}

} // namespace orderbook

#endif // ORDERBOOK_TYPES_HPP
