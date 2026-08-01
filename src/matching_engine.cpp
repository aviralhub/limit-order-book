#include "orderbook/matching_engine.hpp"

#include <algorithm>

namespace orderbook {

bool MatchingEngine::submit(Order order, std::vector<Trade>& trades) {
    if (order.quantity == 0 || book_.hasOrder(order.id)) {
        return false;
    }

    const Side other = order.side == Side::Buy ? Side::Sell : Side::Buy;
    while (order.quantity > 0) {
        const std::optional<Order> maker = book_.front(other);
        if (!maker) {
            break;
        }
        const bool crosses =
            order.side == Side::Buy ? order.price >= maker->price : order.price <= maker->price;
        if (!crosses) {
            break;
        }

        const Quantity fill = std::min(order.quantity, maker->quantity);
        trades.push_back(Trade{maker->id, order.id, maker->price, fill});
        order.quantity -= fill;

        if (fill == maker->quantity) {
            book_.cancelOrder(maker->id);
        } else {
            // partial fill keeps its place at the front
            book_.modifyOrder(maker->id, maker->quantity - fill);
        }
    }

    if (order.quantity > 0) {
        book_.addLimitOrder(order);
    }
    return true;
}

} // namespace orderbook
