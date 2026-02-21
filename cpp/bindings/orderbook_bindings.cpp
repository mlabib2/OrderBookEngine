#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "order_book.hpp"
#include "order.hpp"
#include "trade.hpp"
#include "types.hpp"

namespace py = pybind11;
using namespace orderbook;

// Each Order needs a stable memory address while it's on the book.
// We keep a counter here so every order gets a unique ID.
static OrderId g_next_id = 1;

PYBIND11_MODULE(orderbook_engine, m) {
    m.doc() = "Low-latency order book engine";

    // ----------------------------------------------------------------
    // Expose the Trade struct so Python can read trade results
    // ----------------------------------------------------------------
    py::class_<Trade>(m, "Trade")
        .def_readonly("id",            &Trade::id)
        .def_readonly("symbol",        &Trade::symbol)
        .def_readonly("quantity",      &Trade::quantity)
        .def_readonly("buy_order_id",  &Trade::buy_order_id)
        .def_readonly("sell_order_id", &Trade::sell_order_id)
        .def("price", [](const Trade& t) {
            return price_to_double(t.price);
        })
        .def("__repr__", [](const Trade& t) {
            return t.symbol + " qty=" + std::to_string(t.quantity)
                   + " @ $" + std::to_string(price_to_double(t.price));
        });

    // ----------------------------------------------------------------
    // Expose the OrderBook class
    // add_order takes plain Python values — no pointers needed
    // ----------------------------------------------------------------
    py::class_<OrderBook>(m, "OrderBook")
        .def(py::init<const std::string&>(), py::arg("symbol"))

        // add_order: Python passes side/price/qty, we build the Order in C++
        .def("add_order", [](OrderBook& book,
                              const std::string& side,
                              double price,
                              uint64_t quantity) {

            Side s = (side == "buy") ? Side::Buy : Side::Sell;

            // Order is heap-allocated so it outlives this call
            auto* order = new Order(
                g_next_id++,
                book.symbol(),
                s,
                OrderType::Limit,
                quantity,
                price_to_fixed(price)
            );

            return book.add_order(order);
        },
        py::arg("side"), py::arg("price"), py::arg("quantity"))

        .def("cancel_order", &OrderBook::cancel_order, py::arg("order_id"))
        .def("best_bid", [](const OrderBook& book) {
            auto bid = book.best_bid();
            return bid ? py::object(py::float_(price_to_double(*bid))) : py::none();
        })
        .def("best_ask", [](const OrderBook& book) {
            auto ask = book.best_ask();
            return ask ? py::object(py::float_(price_to_double(*ask))) : py::none();
        })
        .def("order_count", &OrderBook::order_count)
        .def("spread", [](const OrderBook& book) {
            auto s = book.spread();
            return s ? py::object(py::float_(price_to_double(*s))) : py::none();
        });
}
