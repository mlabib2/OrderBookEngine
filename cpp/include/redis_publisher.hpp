#ifndef ORDERBOOK_REDIS_PUBLISHER_HPP
#define ORDERBOOK_REDIS_PUBLISHER_HPP

#include "trade.hpp"
#include <hiredis/hiredis.h>
#include <string>

namespace orderbook {

// Publishes trade events to a Redis pub/sub channel.
// One job: take a Trade, send it to Redis.
class RedisPublisher {
public:
    RedisPublisher(const std::string& host = "127.0.0.1", int port = 6379);
    ~RedisPublisher();

    // Returns true if connected to Redis successfully
    bool is_connected() const noexcept;

    // Publish a trade to the "trades" channel
    void publish_trade(const Trade& trade);

private:
    redisContext* ctx_ = nullptr;
};

} // namespace orderbook

#endif // ORDERBOOK_REDIS_PUBLISHER_HPP
