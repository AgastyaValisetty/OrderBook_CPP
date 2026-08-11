# OrderBookCPP

A **price-time–priority limit order book engine** written in modern C++ (C++20+). It models the core matching logic of an electronic trading venue: resting *bids* and *asks*, market/cancel-style order semantics, immediate crossing, and per-level market depth snapshots — packaged as a small, header-driven library plus a runnable demo in `main.cpp`.

The engine is self-contained (no external dependencies beyond the C++ standard library) and is intended as a clean, readable reference implementation of continuous double-auction order matching.

---

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Build](#build)
  - [CLion](#clion)
  - [Command line (CMake)](#command-line-cmake)
  - [Compiler notes & troubleshooting](#compiler-notes--troubleshooting)
- [Project structure](#project-structure)
- [Core concepts](#core-concepts)
  - [Domain types](#domain-types)
  - [Order types](#order-types)
  - [Order priority](#order-priority)
- [Architecture & design](#architecture--design)
  - [Data structures](#data-structures)
  - [The matching algorithm](#the-matching-algorithm)
  - [Order lifecycle](#order-lifecycle)
  - [Threading model](#threading-model)
- [Public API reference](#public-api-reference)
  - [`Orderbook`](#orderbook)
  - [`Order`](#order)
  - [`OrderModify`](#ordermodify)
  - [`Trade` / `TradeInfo`](#trade--tradeinfo)
  - [`OrderbookLevelInfos` / `LevelInfo`](#orderbooklevelinfos--levelinfo)
  - [`Constants`](#constants)
- [Demo program](#demo-program)
  - [Scenario](#scenario)
  - [Expected output](#expected-output)
- [Design notes & trade-offs](#design-notes--trade-offs)
- [Known issues & limitations](#known-issues--limitations)
- [License](#license)

---

## Features

- **Price–time priority matching** — resting orders are queued FIFO within each price level.
- **Five order types** — Good-Till-Cancel, Fill-and-Kill, Fill-or-Kill, Good-for-Day, and Market.
- **Immediate crossing on add** — an inbound order that crosses the top of the opposite book matches instantly and produces a list of executed `Trade`s.
- **Per-level depth snapshots** — `GetOrderInfos()` returns aggregated bid/ask depth (`OrderbookLevelInfos`).
- **O(1) order lookup and cancellation** — by numeric `OrderId`.
- **Efficient "could it fully fill?" pre-checks** — `CanFullyFill` / `CanMatch` back Fill-or-Kill and Fill-and-Kill semantics using per-level running aggregates.
- **Good-for-Day cleanup thread** — a background thread cancels resting Good-for-Day orders at 16:00 local time.
- **Thread-safe public interface** — all public `Orderbook` methods are guarded by a single `std::mutex`.
- **No external dependencies** — standard library only.

---

## Requirements

| Dependency | Requirement |
| --- | --- |
| Compiler | GCC ≥ 13 (with C++20 library support), Clang ≥ 16, or MSVC ≥ 2022 17.4 |
| C++ standard | **C++20 or newer** (the code uses `std::format`, structured bindings, `std::scoped_lock`, `std::optional`) |
| Build system | CMake ≥ 4.2 as declared in `CMakeLists.txt` (the floor can be lowered for older CMake — the code itself only needs CMake's C++ standard handling) |

> The project's `CMakeLists.txt` sets `CMAKE_CXX_STANDARD 26`. Older compilers simply clamp to the highest standard they support — e.g. GCC 13 builds with `-std=gnu++23`, which is sufficient.

---

## Build

### CLion

1. Open the project root as a CLion project (it uses the bundled CMake + MinGW toolchain).
2. Build the `OrderBookCPP` target (Run ▸ Build or `Ctrl+F9`).
3. Run the `OrderBookCPP` executable to exercise the `main.cpp` demo.

### Command line (CMake)

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

Run the demo (Windows):

```bat
OrderBookCPP.exe
```

or (Unix-like):

```bash
./OrderBookCPP
```

### Compiler notes & troubleshooting

- **Use a modern toolchain.** This repository previously carried a `build/` directory configured against an old MinGW GCC **6.3.0**, which *cannot* compile this code (`std::format` and structured bindings require C++20). Configure a fresh build directory with GCC ≥ 13, Clang ≥ 16, or MSVC 2022.
- **CLion's bundled MinGW and `PATH`.** If you invoke CLion's `g++.exe` from a plain shell, it fails silently (exit code 1, no diagnostics) unless the compiler's `bin` directory is on `PATH` — its `cc1plus.exe` DLLs live there. Add `<CLion>/bin/mingw/bin` to `PATH`, or simply build from inside CLion, which sets this up automatically.
- `test.cpp` is a **legacy standalone prototype** (its own copy of every type plus its own `main()`). It is *not* part of the build target and is kept only as historical reference.

---

## Project structure

```
OrderBookCPP/
├── CMakeLists.txt            # CMake build configuration
├── main.cpp                  # Runnable demo: exercises add/match/cancel & prints the book
├── Orderbook.h               # Orderbook class declaration (public API + internals)
├── Orderbook.cpp             # Orderbook implementation (matching, threading, depth)
├── Order.h                   # Order entity + OrderPointer / OrderPointers aliases
├── OrderModify.h             # Immutable change request used to amend an existing order
├── OrderbookLevelInfos.h     # Depth snapshot container (bids + asks)
├── LevelInfo.h               # Single price-level aggregation (price, quantity)
├── Trade.h                   # Trade result + Trades alias
├── TradeInfo.h               # One side of a trade (order id, price, quantity)
├── OrderType.h               # enum class OrderType
├── Side.h                    # enum class Side
├── Usings.h                  # Core domain type aliases
├── Constants.h               # Constants (e.g. InvalidPrice sentinel)
└── test.cpp                  # Legacy prototype (NOT part of the build)
```

---

## Core concepts

### Domain types

Defined in `Usings.h`:

| Alias | Underlying type | Meaning |
| --- | --- | --- |
| `Price` | `std::int32_t` | Order price |
| `Quantity` | `std::uint32_t` | Order size / remaining quantity |
| `OrderId` | `std::uint64_t` | Unique order identifier |
| `OrderIds` | `std::vector<OrderId>` | A set of order IDs (e.g. batch cancellation) |

`Side` (`Side.h`) is `Buy` or `Sell`. `OrderType` (`OrderType.h`) is `GoodTillCancel`, `FillAndKill`, `FillOrKill`, `GoodForDay`, or `Market`.

### Order types

| Order type | Behaviour |
| --- | --- |
| `GoodTillCancel` | Rests on the book until it is fully filled or explicitly cancelled. |
| `FillAndKill` | Must **cross at least partially** at the moment of submission; any unfilled remainder is killed immediately. |
| `FillOrKill` | Must **fully fill** at the moment of submission; otherwise the whole order is killed. |
| `GoodForDay` | Rests on the book like GTC, but is automatically cancelled by the prune thread at 16:00 local time. |
| `Market` | Has no explicit price. On submission it is re-priced to the *worst* price on the opposite side of the book and converted to `GoodTillCancel`, guaranteeing it crosses the entire opposite book. |

### Order priority

Matching follows **price–time priority**:

1. **Price priority** — the best bid (highest) and best ask (lowest) cross first.
2. **Time priority** — within the same price, orders are filled **FIFO** in the order they were added.

---

## Architecture & design

### Data structures

The `Orderbook` (declared in `Orderbook.h`) keeps four containers:

| Member | Type | Purpose |
| --- | --- | --- |
| `bids_` | `std::map<Price, OrderPointers, std::greater<Price>>` | Bid side, sorted **descending** so the best (highest) bid is at the front. |
| `asks_` | `std::map<Price, OrderPointers, std::less<Price>>` | Ask side, sorted **ascending** so the best (lowest) ask is at the front. |
| `orders_` | `std::unordered_map<OrderId, OrderEntry>` | O(1) lookup of any live order by ID, for cancellation and modification. |
| `data_` | `std::unordered_map<Price, LevelData>` | Per-level running totals (`quantity_`, `count_`) used to answer "can this order fully fill?" without walking the price queue. |

Each price level holds an `OrderPointers` — a `std::list<OrderPointer>` (FIFO queue within the level). An `OrderEntry` stores the order plus an **iterator into that list**, so cancellation can erase an order from the middle of a level in O(1).

`LevelData::Action` (`Add`, `Remove`, `Match`) drives the running aggregates in `UpdateLevelData`, keeping `data_` in sync as orders are added, matched, and cancelled.

### The matching algorithm

`MatchOrders()` implements a continuous crossing loop:

1. If either side of the book is empty, stop.
2. Compare the best bid and best ask:
   - If `best bid < best ask`, there is no overlap — stop.
   - Otherwise the market is crossed; match the **oldest** orders at the two top levels.
3. Fill the minimum of the two orders' remaining quantities, `Fill()` both sides, and record a `Trade`.
4. Remove any fully-filled orders from their price levels and from `orders_`.
5. Update the per-level aggregates via `OnOrderMatched`.
6. Erase empty price levels and repeat until the book no longer overlaps.
7. Finally, if the order now at the top of the book is a leftover `FillAndKill`, cancel it.

The algorithm is called from `AddOrder` immediately after an inbound order is placed, so trades execute atomically with the add, and the resulting `Trades` vector is returned to the caller.

### Order lifecycle

- **Add** — `AddOrder(OrderPointer)` validates the ID is unique, applies Market / FAK / FOK pre-checks, inserts the order into the relevant side and `orders_`, updates level data, then runs `MatchOrders()`.
- **Cancel** — `CancelOrder(OrderId)` looks the order up, removes it from its price level and `orders_`, prunes empty levels, and updates the level aggregates.
- **Modify** — `ModifyOrder(OrderModify)` is implemented as **cancel-then-re-add**: the original order's `OrderType` is preserved and the new price/quantity take effect as a fresh order (losing time priority).
- **Expire** — `GoodForDay` orders are swept by the prune thread at 16:00 local time.

### Threading model

- A single `std::mutex` (`ordersMutex_`) serialises all access to book state; every public method takes it.
- On construction, `Orderbook` spawns `ordersPruneThread_`, which runs `PruneGoodForDayOrders()`: it sleeps until 16:00 local time (or until notified), then collects and cancels every resting `GoodForDay` order.
- On destruction the `shutdown_` flag is set, the condition variable is notified, and the prune thread is `join()`ed, guaranteeing clean teardown.
- `Orderbook` is **non-copyable and non-movable** by design (it owns a thread and a mutex).

---

## Public API reference

### `Orderbook`

```cpp
Orderbook();                                    // starts the GoodForDay prune thread
Orderbook(const Orderbook&)            = delete; // non-copyable / non-movable
Orderbook(Orderbook&&)                 = delete;
~Orderbook();                                    // signals shutdown, joins prune thread

Trades                AddOrder(OrderPointer order);     // place an order; returns trades executed
void                  CancelOrder(OrderId orderId);     // cancel a live order
Trades                ModifyOrder(OrderModify order);   // cancel + re-add; returns trades executed
std::size_t           Size() const;                     // number of live orders
OrderbookLevelInfos   GetOrderInfos() const;            // aggregated bid/ask depth snapshot
```

### `Order`

```cpp
Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity);
Order(OrderId orderId, Side side, Quantity quantity);   // creates a Market order (no price)

OrderId     GetOrderId() const;
Side        GetSide() const;
Price       GetPrice() const;
OrderType   GetOrderType() const;
Quantity    GetInitialQuantity() const;
Quantity    GetRemainingQuantity() const;
Quantity    GetFilledQuantity() const;                  // initial − remaining
bool        IsFilled() const;                           // remaining == 0
void        Fill(Quantity quantity);                    // throws std::logic_error on over-fill
void        ToGoodTillCancel(Price price);              // converts a Market order to GTC at a price
```

Aliases: `OrderPointer = std::shared_ptr<Order>`, `OrderPointers = std::list<OrderPointer>`.

### `OrderModify`

An immutable change request for an existing order:

```cpp
OrderModify(OrderId orderId, Side side, Price price, Quantity quantity);
OrderId     GetOrderId() const;
Side        GetSide() const;
Price       GetPrice() const;
Quantity    GetQuantity() const;
OrderPointer GetOrderPointer(OrderType type) const;      // builds the replacement Order
```

### `Trade` / `TradeInfo`

```cpp
struct TradeInfo {
    OrderId  orderId_;
    Price    price_;
    Quantity quantity_;
};

class Trade {
    const TradeInfo& bidTrade() const;    // the resting/aggressive bid side
    const TradeInfo& askTrade() const;    // the resting/aggressive ask side
};

using Trades = std::vector<Trade>;
```

### `OrderbookLevelInfos` / `LevelInfo`

```cpp
struct LevelInfo {
    Price    price;
    Quantity quantity_;   // aggregate remaining quantity at this level
};
using LevelInfos = std::vector<LevelInfo>;

class OrderbookLevelInfos {
    const LevelInfos& GetBids() const;
    const LevelInfos& GetAsks() const;
};
```

### `Constants`

```cpp
struct Constants {
    static const Price InvalidPrice;   // sentinel for orders without a price yet
};
```

`InvalidPrice` is `std::numeric_limits<Price>::max()` — used as the placeholder price of a `Market` order before it is converted to `GoodTillCancel`.

---

## Demo program

`main.cpp` walks through a short matching session, printing trades and the book after each step.

### Scenario

1. **BUY 100 @ 100** — rests on the book (no asks to cross).
2. **BUY 50 @ 99** — rests below the existing 100 bid.
3. **SELL 70 @ 101** — rests on the ask side (best bid 100 < ask 101, no cross).
4. **SELL 80 @ 100** — *crosses immediately*: the best bid (100) is now ≥ the best ask (100). The sell matches the resting 100 @ 100 bid for the minimum quantity (80), fully filling the sell and leaving 20 on the bid.
5. **SELL 100 @ 100 (FillAndKill)** — *crosses immediately* against the remaining 20 @ 100 bid and fills 20. The unfilled remainder (80) is then **killed** rather than allowed to rest — demonstrating Fill-and-Kill semantics.

### Expected output

```
No trades executed.


========== ORDER BOOK ==========

ASKS

BIDS
100  Qty: 100
================================

No trades executed.


========== ORDER BOOK ==========

ASKS

BIDS
100  Qty: 100
99  Qty: 50
================================

No trades executed.


========== ORDER BOOK ==========

ASKS
101  Qty: 70

BIDS
100  Qty: 100
99  Qty: 50
================================


Trades Executed
=========================
BUY Order 1 <--> SELL Order 4 | Price = 100 | Qty = 80


========== ORDER BOOK ==========

ASKS
101  Qty: 70

BIDS
100  Qty: 20
99  Qty: 50
================================


Trades Executed
=========================
BUY Order 1 <--> SELL Order 5 | Price = 100 | Qty = 20


========== ORDER BOOK ==========

ASKS
101  Qty: 70

BIDS
99  Qty: 50
================================
```

`PrintTrades` reports the **resting order's** price for each trade — matching at the resting price is the conventional continuous-auction behaviour. `PrintBook` prints the depth snapshot returned by `GetOrderInfos()`.

In step 5 the aggressive FAK sell rests momentarily at the touch with its unfilled 80-lot remainder, which `MatchOrders` then kills — so the sell order never appears in the final depth snapshot (only its 20-lot fill does, as the trade above).

---

## Design notes & trade-offs

- **A single global mutex** keeps the implementation simple and obviously correct, but it serialises all operations. A lock-free or fine-grained design (per-side or per-level locks) would scale better under high concurrency; this project optimises for clarity.
- **`data_` / `LevelData`** trades a small amount of bookkeeping on every add/remove/match for O(number of levels) `CanFullyFill` checks, which makes `FillOrKill` evaluation cheap.
- **Market orders as worst-price GTC** is a deliberate simplification: a market buy is re-priced to the highest ask (and vice-versa) so it necessarily crosses the full opposite book; any unfilled remainder continues to rest as a GTC order rather than being killed.
- **Modify = cancel + re-add** is simple but sacrifices time priority and re-runs matching; a true modify would edit the resting order in place.
- The class is **non-copyable** because it owns a thread and a mutex.

---

## Known issues & limitations

- **FAK partial-fill handling (deadlock — fixed).** A `FillAndKill` order that crosses partially leaves an unfilled remainder that must be killed immediately. Earlier code invoked the locking `CancelOrder()` from inside `MatchOrders()` — while `AddOrder()` already held the non-recursive `ordersMutex_` — which deadlocked on any partially-filled FAK order. The internal cancellation now goes through the non-locking `CancelOrderInternal()`, which is safe because `MatchOrders()` runs with the mutex already held. The `main.cpp` demo exercises this path in step 5.
- **`data_` aggregate accounting (underflow — fixed).** `data_` counts *both* sides of the book at a price level, so a price that has just emptied on one side may still be occupied on the other (e.g. an aggressive bid fully fills while a deeper resting ask sits at the same price, or an unfilled FAK remainder rests at the crossed price). Earlier code explicitly erased `data_` entries in `MatchOrders()` when one side emptied; if the other side still rested at that price, the entry was dropped and `UpdateLevelData` would later re-create it with a negative `count_`. `MatchOrders()` no longer erases `data_` at all — `UpdateLevelData` drops an entry on its own once its `count_` reaches `0`, which is always correct because every remove (match / cancel / FAK-kill) decrements the count.
- **FOK / FAK with the empty side.** If the opposite book is empty, these orders are rejected outright, which is correct behaviour.
- **Single-asset, no persistence, no network/FIX layer.** The engine covers the in-memory matching core only; execution reporting, risk checks, and an order gateway are out of scope.
- **16:00 local-time cutoff** for `GoodForDay` orders uses the system local timezone and is not configurable.

---

## License
(MIT, Apache-2.0, GPL-3.0, etc.) — the repository does not currently ship a `LICENSE` file.
