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

// price is the resting (maker) order's price
struct Trade {
    OrderId maker_id;
    OrderId taker_id;
    Price price;
    Quantity quantity;

    bool operator==(const Trade&) const = default;
};

} // namespace orderbook
