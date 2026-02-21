#include "order_book.hpp"
#include "redis_publisher.hpp"
#include "order.hpp"
#include <iostream>

using namespace orderbook;

int main() {
    // Connect to Redis
    RedisPublisher publisher;
    if (!publisher.is_connected()) {
        std::cerr << "Could not connect to Redis\n";
        return 1;
    }
    std::cout << "Connected to Redis.\n";

    // Create an order book for AAPL
    OrderBook book("AAPL");

    OrderId next_id = 1;

    // Add a resting sell order: 100 shares @ $101.00
    Order sell(next_id++, "AAPL", Side::Sell, OrderType::Limit, 100, price_to_fixed(101.0));
    book.add_order(&sell);
    std::cout << "Added SELL 100 @ $101.00 (resting on book)\n";

    // Add an aggressive buy order: 100 shares @ $102.00
    // This crosses the spread → triggers a match → generates a trade
    Order buy(next_id++, "AAPL", Side::Buy, OrderType::Limit, 100, price_to_fixed(102.0));
    auto trades = book.add_order(&buy);
    std::cout << "Added BUY  100 @ $102.00 (crosses spread)\n";

    // Publish each trade to Redis
    for (const auto& trade : trades) {
        publisher.publish_trade(trade);
        std::cout << "Published trade: "
                  << trade.symbol << " "
                  << trade.quantity << " @ $"
                  << price_to_double(trade.price) << "\n";
    }

    return 0;
}
