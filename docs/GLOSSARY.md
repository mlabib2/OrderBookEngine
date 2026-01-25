# Glossary: Trading Terms

## Basic Concepts

### Order Book
A list of all outstanding buy and sell orders for a financial instrument, organized by price level. The order book shows the supply and demand at each price.

### Bid
An order to BUY. "I bid $100 for 50 shares" means "I want to buy 50 shares at $100."

### Ask (or Offer)
An order to SELL. "I ask $101 for 50 shares" means "I want to sell 50 shares at $101."

### Best Bid
The highest price any buyer is willing to pay. This is always the first (top) bid in the order book.

### Best Ask
The lowest price any seller is willing to accept. This is always the first (top) ask in the order book.

### Spread
The difference between the best ask and best bid.
```
Spread = Best Ask - Best Bid
Example: If best bid = $100.00 and best ask = $100.25, spread = $0.25
```

### Mid Price
The average of the best bid and best ask.
```
Mid Price = (Best Bid + Best Ask) / 2
Example: ($100.00 + $100.25) / 2 = $100.125
```

---

## Order Types

### Limit Order
An order to buy or sell at a specific price or better.
- **Buy Limit**: Execute at the limit price or LOWER
- **Sell Limit**: Execute at the limit price or HIGHER
- If no immediate match, the order "rests" on the book

### Market Order
An order to buy or sell immediately at the best available price.
- Always executes (if there's liquidity)
- May execute at multiple price levels
- No price guarantee

---

## Matching Concepts

### Price-Time Priority (FIFO)
The standard matching algorithm for most exchanges:

1. **Price Priority**: Better prices match first
   - For incoming BUY: match against lowest ASK first
   - For incoming SELL: match against highest BID first

2. **Time Priority**: At the same price, earlier orders match first
   - First In, First Out (FIFO)
   - Rewards market participants who provide liquidity early

### Trade
When a buy order matches with a sell order, a trade occurs. A trade has:
- Price (the execution price)
- Quantity (how much was exchanged)
- Buyer and seller order IDs
- Timestamp

### Partial Fill
When an order doesn't fully match in one trade.
```
Example:
- Resting sell order: 100 shares at $50
- Incoming buy order: 150 shares at $50
- Result: Trade of 100 shares, buy order has 50 shares remaining
```

### Aggressor vs Passive
- **Aggressor**: The incoming order that crosses the spread
- **Passive/Resting**: Orders already on the book waiting to be matched

---

## Order Lifecycle

### Order States
```
NEW → PARTIALLY_FILLED → FILLED
                      ↘
NEW ─────────────────────→ CANCELLED
```

- **New**: Order received, on the book (or about to be matched)
- **Partially Filled**: Some quantity executed, rest still on book
- **Filled**: Entire quantity executed
- **Cancelled**: Order removed before full execution

---

## Price Levels

### Level 1 (L1)
Just the best bid and best ask with their quantities.
```
Best Bid: $100.00 (500 shares)
Best Ask: $100.25 (300 shares)
```

### Level 2 (L2)
Multiple price levels deep on each side.
```
BIDS                    ASKS
$100.00  500 shares     $100.25  300 shares
$99.75   200 shares     $100.50  450 shares
$99.50   800 shares     $100.75  200 shares
```

### Depth
How many price levels are shown. "10 levels deep" means showing the best 10 bid and ask prices.

---

## Performance Metrics

### Latency
Time delay between an action and its result.
```
Order latency = Time from order submission to acknowledgment
Match latency = Time to execute matching algorithm
```

### Throughput
Number of operations per unit time.
```
100,000 orders/second = processing 100K order submissions per second
```

---

## Example Order Book

```
        BIDS                          ASKS
Price     Qty    Orders      Price     Qty    Orders
────────────────────────────────────────────────────
                            $101.00   200    [J]
                            $100.75   150    [H, I]
                            $100.50   100    [G]      ← Best Ask
────────────────────────────────────────────────────
$100.25   300    [D, E, F]                           ← Best Bid
$100.00   200    [B, C]
$99.75    100    [A]

Spread: $100.50 - $100.25 = $0.25
Mid Price: ($100.50 + $100.25) / 2 = $100.375
```

**Scenario**: New BUY order for 250 shares at $100.75

1. Matches 100 shares with order G at $100.50 (best ask)
2. Matches 150 shares with orders H, I at $100.75
3. Order fully filled, no remainder on book

Result: Two trades executed, buyer paid $100.50 for first 100, $100.75 for next 150.
