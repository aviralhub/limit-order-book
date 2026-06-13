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

Debug builds run with ASan and UBSan.

## Layout

```
include/orderbook/   Order, OrderBook
src/                 implementation
tests/               Catch2 tests
```
