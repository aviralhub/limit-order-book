#include "orderbook/order_book.hpp"

namespace orderbook {

void OrderBook::addLimitOrder(const Order& order) {
    PriceLevel& level = (order.side == Side::Buy) ? bids_[order.price] : asks_[order.price];
    level.orders.push_back(order);
    level.total_quantity += order.quantity;
    locations_.emplace(order.id, OrderLocation{order.side, order.price, &level,
                                               std::prev(level.orders.end())});
}

bool OrderBook::cancelOrder(OrderId id) {
    auto loc_it = locations_.find(id);
    if (loc_it == locations_.end()) {
        return false;
    }
    const OrderLocation& loc = loc_it->second;

    PriceLevel& level = *loc.level;
    level.total_quantity -= loc.it->quantity;
    level.orders.erase(loc.it);

    if (level.orders.empty()) {
        if (loc.side == Side::Buy) {
            bids_.erase(loc.price);
        } else {
            asks_.erase(loc.price);
        }
    }

    locations_.erase(loc_it);
    return true;
}

bool OrderBook::modifyOrder(OrderId id, Quantity new_quantity) {
    auto loc_it = locations_.find(id);
    if (loc_it == locations_.end()) {
        return false;
    }
    const OrderLocation& loc = loc_it->second;

    PriceLevel& level = *loc.level;
    level.total_quantity = level.total_quantity - loc.it->quantity + new_quantity;
    loc.it->quantity = new_quantity;
    return true;
}

template <typename Levels>
std::optional<Price> OrderBook::bestPrice(const Levels& levels) {
    if (levels.empty()) {
        return std::nullopt;
    }
    return levels.begin()->first;
}

template <typename Levels>
Quantity OrderBook::quantityAtIn(const Levels& levels, Price price) {
    auto it = levels.find(price);
    return it == levels.end() ? 0 : it->second.total_quantity;
}

template <typename Levels>
std::vector<OrderId> OrderBook::ordersAtIn(const Levels& levels, Price price) {
    std::vector<OrderId> ids;
    auto it = levels.find(price);
    if (it == levels.end()) {
        return ids;
    }
    ids.reserve(it->second.orders.size());
    for (const Order& order : it->second.orders) {
        ids.push_back(order.id);
    }
    return ids;
}

std::optional<Price> OrderBook::bestBid() const { return bestPrice(bids_); }

std::optional<Price> OrderBook::bestAsk() const { return bestPrice(asks_); }

Quantity OrderBook::quantityAt(Side side, Price price) const {
    return side == Side::Buy ? quantityAtIn(bids_, price) : quantityAtIn(asks_, price);
}

std::vector<OrderId> OrderBook::ordersAt(Side side, Price price) const {
    return side == Side::Buy ? ordersAtIn(bids_, price) : ordersAtIn(asks_, price);
}

} // namespace orderbook
