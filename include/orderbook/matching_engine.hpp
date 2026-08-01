#pragma once

#include "orderbook/order.hpp"
#include "orderbook/order_book.hpp"

#include <cstddef>
#include <vector>

namespace orderbook {

class MatchingEngine {
public:
    explicit MatchingEngine(std::size_t capacity = 1 << 16) : book_(capacity) {}

    // Matches against the other side, best price first and oldest first within
    // a price, then rests whatever is left. Trades are appended to `trades`.
    // False for a zero quantity or an id that's already resting.
    bool submit(Order order, std::vector<Trade>& trades);

    bool cancel(OrderId id) { return book_.cancelOrder(id); }

    const OrderBook& book() const { return book_; }

private:
    OrderBook book_;
};

} // namespace orderbook
