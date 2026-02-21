#include <benchmark/benchmark.h>
#include "order_book.hpp"
#include "order.hpp"
#include "types.hpp"
#include <vector>

using namespace orderbook;

// ============================================================================
// Helpers
// ============================================================================

static constexpr int POOL = 10'000;

static void reset_orders(std::vector<Order>& orders) {
    for (auto& o : orders) {
        o.filled_quantity = 0;
        o.status = OrderStatus::New;
    }
}

static std::vector<Order> make_limit_orders(int n, OrderId id_start,
                                             Side side, double base_price) {
    std::vector<Order> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i) {
        // Spread across 100 price levels (0.01 tick size)
        double price = base_price + (side == Side::Sell ? 1 : -1) * (i % 100) * 0.01;
        v.emplace_back(id_start + static_cast<OrderId>(i),
                       "AAPL", side, OrderType::Limit, 100ULL,
                       price_to_fixed(price));
    }
    return v;
}

// ============================================================================
// BM_AddOrder
// Measures: latency to add a non-matching limit order to a live book.
// Resume claim: "10µs order insertion"
// ============================================================================
static void BM_AddOrder(benchmark::State& state) {
    auto orders = make_limit_orders(POOL, 1, Side::Buy, 99.0);
    OrderBook book("AAPL");
    int64_t idx = 0;

    for (auto _ : state) {
        // Refresh book every POOL iterations to avoid unbounded growth
        if (idx > 0 && idx % POOL == 0) {
            state.PauseTiming();
            book = OrderBook("AAPL");
            reset_orders(orders);
            state.ResumeTiming();
        }
        benchmark::DoNotOptimize(book.add_order(&orders[idx % POOL]));
        ++idx;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_AddOrder)->Unit(benchmark::kMicrosecond);

// ============================================================================
// BM_CancelOrder
// Measures: latency to cancel a resting order (O(1) via lookup map).
// Resume claim: "O(1) cancel"
// ============================================================================
static void BM_CancelOrder(benchmark::State& state) {
    auto orders = make_limit_orders(POOL, 1, Side::Buy, 99.0);

    auto repopulate = [&](OrderBook& book) {
        reset_orders(orders);
        for (auto& o : orders) book.add_order(&o);
    };

    OrderBook book("AAPL");
    repopulate(book);
    int64_t idx = 0;

    for (auto _ : state) {
        if (idx > 0 && idx % POOL == 0) {
            state.PauseTiming();
            book = OrderBook("AAPL");
            repopulate(book);
            state.ResumeTiming();
        }
        benchmark::DoNotOptimize(book.cancel_order(orders[idx % POOL].id));
        ++idx;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_CancelOrder)->Unit(benchmark::kMicrosecond);

// ============================================================================
// BM_MatchOrder
// Measures: latency when an incoming order fully matches a resting order.
// Resume claim: "100K orders/sec throughput"
// Each buy at 102.0 matches exactly one resting sell at 101.0 (1:1 fill).
// ============================================================================
static void BM_MatchOrder(benchmark::State& state) {
    static const Price SELL_PRICE = price_to_fixed(101.0);
    static const Price BUY_PRICE  = price_to_fixed(102.0);

    std::vector<Order> sell_orders, buy_orders;
    sell_orders.reserve(POOL);
    buy_orders.reserve(POOL);

    for (int i = 0; i < POOL; ++i) {
        sell_orders.emplace_back(static_cast<OrderId>(i + 1),
                                 "AAPL", Side::Sell, OrderType::Limit, 100ULL, SELL_PRICE);
        buy_orders.emplace_back(static_cast<OrderId>(POOL + i + 1),
                                "AAPL", Side::Buy, OrderType::Limit, 100ULL, BUY_PRICE);
    }

    auto repopulate = [&](OrderBook& book) {
        reset_orders(sell_orders);
        for (auto& o : sell_orders) book.add_order(&o);
    };

    OrderBook book("AAPL");
    repopulate(book);
    int64_t idx = 0;

    for (auto _ : state) {
        if (idx > 0 && idx % POOL == 0) {
            state.PauseTiming();
            book = OrderBook("AAPL");
            repopulate(book);
            reset_orders(buy_orders);
            state.ResumeTiming();
        }
        benchmark::DoNotOptimize(book.add_order(&buy_orders[idx % POOL]));
        ++idx;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchOrder)->Unit(benchmark::kMicrosecond);

// ============================================================================
// BM_BestBidAsk
// Measures: latency to query top-of-book (O(1)).
// ============================================================================
static void BM_BestBidAsk(benchmark::State& state) {
    const int N = 1000;
    auto buys  = make_limit_orders(N, 1,     Side::Buy,  99.0);
    auto sells = make_limit_orders(N, N + 1, Side::Sell, 101.0);

    OrderBook book("AAPL");
    for (auto& o : buys)  book.add_order(&o);
    for (auto& o : sells) book.add_order(&o);

    for (auto _ : state) {
        benchmark::DoNotOptimize(book.best_bid());
        benchmark::DoNotOptimize(book.best_ask());
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_BestBidAsk)->Unit(benchmark::kNanosecond);

BENCHMARK_MAIN();
