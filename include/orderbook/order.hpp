#pragma once

#include <cstdint>

namespace orderbook {

enum class Side : std::uint8_t { Buy, Sell };

using OrderId = std::uint64_t;
using Price = std::int64_t;      // ticks, never floating point
using Quantity = std::uint32_t;

struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;
};

} // namespace orderbook
