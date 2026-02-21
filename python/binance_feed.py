import websocket
import json
import sys
import os

# Add the build directory so Python can find the compiled C++ module
sys.path.append(os.path.join(os.path.dirname(__file__), "../cpp/build"))
import orderbook_engine

# Binance WebSocket URL for BTCUSDT top-10 order book depth
URL = "wss://stream.binance.com:9443/ws/btcusdt@depth10@100ms"

# One order book for BTCUSDT, shared across all messages
book = orderbook_engine.OrderBook("BTCUSDT")


def on_message(ws, message):
    data = json.loads(message)

    # Each bid/ask is [price_str, quantity_str]
    # We take only the best bid and best ask (first entry)
    best_bid = data["bids"][0]
    best_ask = data["asks"][0]

    bid_price = float(best_bid[0])
    bid_qty   = int(float(best_bid[1]) * 1000)  # scale: 0.5 BTC → 500 units

    ask_price = float(best_ask[0])
    ask_qty   = int(float(best_ask[1]) * 1000)

    # Feed into C++ engine
    book.add_order("buy",  bid_price, bid_qty)
    book.add_order("sell", ask_price, ask_qty)

    # Print current top of book
    print(f"BID {bid_price:.2f} x {bid_qty}  |  ASK {ask_price:.2f} x {ask_qty}"
          f"  |  spread = {book.spread():.2f}" if book.spread() else "")


def on_error(ws, error):
    print(f"Error: {error}")


def on_close(ws, close_status_code, close_msg):
    print("Connection closed")


def on_open(ws):
    print(f"Connected to Binance: {URL}")


if __name__ == "__main__":
    ws = websocket.WebSocketApp(
        URL,
        on_open=on_open,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close,
    )
    ws.run_forever()
