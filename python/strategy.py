"""
Market-making strategy — uses order book state, not moving averages.

Logic:
- Compute mid price = (best_bid + best_ask) / 2
- Post a buy  limit just below mid (bid = mid - half_spread)
- Post a sell limit just above mid (ask = mid + half_spread)
- If both sides fill, we collect the spread as profit
- Skew quotes away from our heavy side when inventory gets too large
"""

MAX_POSITION  = 5.0    # maximum BTC to hold (inventory risk cap)
MIN_CASH      = 1_000.0
HALF_SPREAD   = 0.001  # post 0.1% away from mid on each side


class MarketMaker:
    def __init__(self):
        pass

    def on_tick(self, best_bid: float, best_ask: float,
                btc_held: float, cash: float) -> str:
        """
        Called once per tick with current order book top-of-book.
        Returns "buy", "sell", or "hold".
        """
        if best_bid is None or best_ask is None:
            return "hold"

        mid = (best_bid + best_ask) / 2

        # Inventory skew — if we're too long, only sell; too short, only buy
        if btc_held >= MAX_POSITION:
            return "sell"

        if cash <= MIN_CASH:
            return "hold"

        # Core market-making logic: buy below mid, sell above mid
        our_bid = mid * (1 - HALF_SPREAD)
        our_ask = mid * (1 + HALF_SPREAD)

        if best_ask <= our_ask and cash >= best_ask:
            # Someone is willing to sell at or below our ask — lift the offer
            return "buy"

        if best_bid >= our_bid and btc_held > 0:
            # Someone is willing to buy at or above our bid — hit the bid
            return "sell"

        return "hold"
