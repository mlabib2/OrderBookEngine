#include "redis_publisher.hpp"
#include <stdexcept>
#include <string>

namespace orderbook {

// Connect to Redis when the object is created
RedisPublisher::RedisPublisher(const std::string& host, int port) {
    ctx_ = redisConnect(host.c_str(), port);

    if (ctx_ == nullptr || ctx_->err) {
        std::string err = ctx_ ? ctx_->errstr : "allocation failure";
        if (ctx_) redisFree(ctx_);
        ctx_ = nullptr;
        throw std::runtime_error("Redis connection failed: " + err);
    }
}

// Disconnect when the object is destroyed
RedisPublisher::~RedisPublisher() {
    if (ctx_) {
        redisFree(ctx_);
        ctx_ = nullptr;
    }
}

bool RedisPublisher::is_connected() const noexcept {
    return ctx_ != nullptr;
}

void RedisPublisher::publish_trade(const Trade& trade) {
    if (!is_connected()) return;

    // Build the message string:
    // "symbol=AAPL price=101.000000 qty=100 buy=1 sell=2"
    std::string msg =
        "symbol=" + trade.symbol +
        " price=" + std::to_string(price_to_double(trade.price)) +
        " qty="   + std::to_string(trade.quantity) +
        " buy="   + std::to_string(trade.buy_order_id) +
        " sell="  + std::to_string(trade.sell_order_id);

    // PUBLISH trades "<msg>"
    redisCommand(ctx_, "PUBLISH trades %s", msg.c_str());
}

} // namespace orderbook
