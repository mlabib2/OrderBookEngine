"""
Backtester — replays historical BTCUSDT daily data through the C++ engine
and measures strategy performance.

Usage:
    python python/backtest.py
"""

import sys
import os
import math

import yfinance as yf
import pandas as pd

sys.path.append(os.path.join(os.path.dirname(__file__), "../cpp/build"))
import orderbook_engine

# ── Strategy import ────────────────────────────────────────────────────────────
from strategy import MarketMaker


# ── Data ───────────────────────────────────────────────────────────────────────
def fetch_data(ticker: str = "BTC-USD", period: str = "1y") -> pd.DataFrame:
    """Download historical daily OHLCV data from Yahoo Finance."""
    df = yf.download(ticker, period=period, auto_adjust=True, progress=False)
    # Newer yfinance returns a MultiIndex — flatten to plain column names
    if isinstance(df.columns, pd.MultiIndex):
        df.columns = df.columns.get_level_values(0)
    df = df[["Open", "High", "Low", "Close", "Volume"]].dropna()
    return df


# ── Backtest engine ────────────────────────────────────────────────────────────
def run(df: pd.DataFrame, starting_cash: float = 100_000.0) -> dict:
    cash = starting_cash
    btc_held = 0.0
    portfolio_values = []
    peak = starting_cash

    max_drawdown = 0.0
    daily_returns = []
    prev_value = starting_cash

    strategy = MarketMaker()
    book = orderbook_engine.OrderBook("BTC-USD")

    for date, row in df.iterrows():
        price = float(row["Close"])
        spread = price * 0.001          # simulate 0.1% bid/ask spread
        bid = price - spread / 2
        ask = price + spread / 2
        qty = 1                          # trade 1 BTC unit at a time

        # Feed current market into the C++ engine
        book.add_order("buy",  bid, qty)
        book.add_order("sell", ask, qty)

        # Ask strategy what to do — pass order book state, not just price
        action = strategy.on_tick(book.best_bid(), book.best_ask(),
                                  btc_held, cash)

        if action == "buy" and cash >= ask:
            trades = book.add_order("buy", ask, qty)
            if trades:
                cash     -= ask
                btc_held += qty

        elif action == "sell" and btc_held >= qty:
            trades = book.add_order("sell", bid, qty)
            if trades:
                cash     += bid
                btc_held -= qty

        # Mark-to-market portfolio value
        portfolio_value = cash + btc_held * price
        portfolio_values.append(portfolio_value)

        # Max drawdown
        if portfolio_value > peak:
            peak = portfolio_value
        drawdown = (peak - portfolio_value) / peak
        if drawdown > max_drawdown:
            max_drawdown = drawdown

        # Daily return
        daily_returns.append((portfolio_value - prev_value) / prev_value)
        prev_value = portfolio_value

    # ── Metrics ────────────────────────────────────────────────────────────────
    total_return = (portfolio_values[-1] - starting_cash) / starting_cash

    avg_daily   = sum(daily_returns) / len(daily_returns)
    variance    = sum((r - avg_daily) ** 2 for r in daily_returns) / len(daily_returns)
    std_daily   = math.sqrt(variance)
    sharpe      = (avg_daily / std_daily) * math.sqrt(252) if std_daily > 0 else 0.0

    return {
        "start_value":   starting_cash,
        "end_value":     portfolio_values[-1],
        "total_return":  total_return,
        "sharpe_ratio":  sharpe,
        "max_drawdown":  max_drawdown,
        "days_traded":   len(df),
        "btc_held":      btc_held,
        "cash":          cash,
    }


# ── Report ─────────────────────────────────────────────────────────────────────
def print_report(results: dict) -> None:
    print("\n" + "=" * 45)
    print("  BACKTEST RESULTS — BTCUSDT (1 year daily)")
    print("=" * 45)
    print(f"  Starting capital : ${results['start_value']:>12,.2f}")
    print(f"  Ending value     : ${results['end_value']:>12,.2f}")
    print(f"  Total return     : {results['total_return']:>11.1%}")
    print(f"  Sharpe ratio     : {results['sharpe_ratio']:>12.2f}")
    print(f"  Max drawdown     : {results['max_drawdown']:>11.1%}")
    print(f"  Days traded      : {results['days_traded']:>12}")
    print(f"  BTC held at end  : {results['btc_held']:>12.4f}")
    print(f"  Cash at end      : ${results['cash']:>12,.2f}")
    print("=" * 45 + "\n")


if __name__ == "__main__":
    print("Fetching 1 year of BTC-USD daily data...")
    df = fetch_data("BTC-USD", period="1y")
    print(f"  {len(df)} trading days loaded.")

    print("Running backtest...")
    results = run(df)
    print_report(results)
