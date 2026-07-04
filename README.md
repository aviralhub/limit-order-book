# limit-order-book

Limit order book in C++20 with price-time priority. One `std::map` of price levels per side, a FIFO
queue of orders at each level, and a hash map from order id to where the order sits, so cancel and
modify don't have to search for it.

Prices are `int64_t` ticks, not doubles.

## Build

GCC or Clang, CMake 3.20+, Ninja. The first configure fetches Catch2.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Debug builds run with ASan and UBSan. CI builds and tests both Debug and Release on every push.

## Benchmark

`build/bench/bench_order_book` fills the book to 100k orders, then times 1M adds, cancels and
modifies with `rdtsc`. Prices are normally distributed around 30000 ticks and the workload is
generated from a fixed seed before the timed loop. i7-1255U, WSL2, GCC 15.2, `-O3 -march=native`:

```
100000 resting orders, 1000000 measured ops (45% add, 45% cancel, 10% modify)
rdtsc 2.611 cycles/ns, timer overhead 20 cycles (included)
1585946 ops/sec, 99583 orders left

op           count     p50     p99   p99.9       max  (ns)
add         449811     530    1345   12097    883985
cancel      450186     183    1524    3871   1435885
modify      100003       9     500    1030   1444089
all        1000000     407    1420    9054   1444089
```

Before `OrderLocation` held a pointer to its level, cancel and modify looked the level up in the
map first, and both were around 750 ns. Add got slower because `OrderLocation` is 8 bytes bigger and
every add writes one into the hash map. The run isn't pinned to a core, so these move by 20% or so
between runs.

## Layout

```
include/orderbook/   Order, OrderBook
src/                 implementation
tests/               Catch2 tests
bench/               latency benchmark
docs/                design notes
```
