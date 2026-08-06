# limit-order-book

Limit order book and matching engine in C++20, with price-time priority. One `std::map` of price
levels per side, a FIFO queue of orders at each level, and a hash map from order id to where the
order sits, so cancel and modify don't have to search for it. Orders live in one pooled vector, so
adding one doesn't allocate.

Prices are `int64_t` ticks, not doubles.

```cpp
MatchingEngine engine;
std::vector<Trade> trades;

engine.submit({.id = 1, .side = Side::Sell, .price = 100, .quantity = 5}, trades);
engine.submit({.id = 2, .side = Side::Sell, .price = 101, .quantity = 5}, trades);
engine.submit({.id = 3, .side = Side::Buy, .price = 101, .quantity = 8}, trades);

// trades: {maker 1, taker 3, price 100, qty 5}, {maker 2, taker 3, price 101, qty 3}
// order 2 rests with 2 left
```

An incoming order trades against the other side at the resting order's price, best price first and
oldest first within a price. Whatever doesn't fill rests in the book.

## Build

GCC or Clang, CMake 3.20+, Ninja. The first configure fetches Catch2.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Debug builds run with ASan and UBSan. CI builds and tests both Debug and Release on every push.
Besides the hand-written cases, `tests/test_randomised.cpp` runs 20k random operations against the
book and the engine and checks every result against a naive model.

## Benchmark

`build/bench/bench_order_book [core]` fills the book to 100k orders, then times 1M adds, cancels and
modifies with `rdtsc`. Prices are normally distributed around 30000 ticks, and the workload comes
from a fixed seed and is generated before the timed loop. i7-1255U, WSL2, GCC 15.2,
`-O3 -march=native`, pinned to core 10.

Medians of 20 runs of each version, run alternately, in ns:

| | v1 list | v2 level pointer | v3 pool |
|---|---:|---:|---:|
| add p50 | 367 | 431 | 341 |
| cancel p50 | 645 | 81 | 291 |
| modify p50 | 603 | 9 | 9 |
| all p50 | 516 | 332 | 298 |
| all p99 | 1459 | 1211 | 1116 |
| all p99.9 | 11895 | 4839 | 2732 |

v1 looked the level up in the map on every cancel and modify. v2 keeps a pointer to the level
instead, which made add slower because every add writes a bigger entry into the hash map.

v3 replaces the `std::list` at each level with one vector of nodes linked by index, plus a free list,
so add doesn't allocate anymore. Cancel is slower than in v2. v2's cancel p50 jumps between 18 and
400 ns from run to run, while v3's stays close to 290, probably because unlinking now touches
neighbours spread through a large array instead of heap nodes that were freed a moment ago. The
p99.9 is what improved most, and that's the reason for keeping it.

Numbers move by 20% or more between sessions, so only runs from the same session are compared.
Pinning to a core didn't change much under WSL2.

The second half of the run goes through `MatchingEngine`. It starts with 100k resting orders, buys
below 30000 and sells above, then does 1M operations: 42% submits priced away from the mid that
rest, 10% priced 20 ticks through the mid that trade, and 48% cancels. Medians of 10 runs, in ns:

| submit | count | p50 | p99 | p99.9 |
|---|---:|---:|---:|---:|
| no fill | 468681 | 242 | 994 | 1965 |
| fills | 50767 | 82 | 698 | 1322 |

A submit that trades is cheaper than one that rests, even with about two trades each. It works on
the front of the best level, which is already in cache, and usually fills completely, so nothing
gets added. One that rests lands somewhere deeper in the book and has to find or create its level
and insert into the hash map.

## Layout

```
include/orderbook/   Order, Trade, OrderBook, MatchingEngine
src/                 implementation
tests/               Catch2 tests
bench/               latency benchmark
docs/                design notes
```
