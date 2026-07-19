#include "orderbook/order_book.hpp"

namespace orderbook {

OrderBook::OrderBook(std::size_t capacity) {
    nodes_.reserve(capacity);
    free_list_.reserve(capacity);
}

std::int32_t OrderBook::allocate(const Order& order) {
    if (!free_list_.empty()) {
        const std::int32_t idx = free_list_.back();
        free_list_.pop_back();
        nodes_[static_cast<std::size_t>(idx)] = Node{order.id, order.quantity, kNil, kNil};
        return idx;
    }
    nodes_.push_back(Node{order.id, order.quantity, kNil, kNil});
    return static_cast<std::int32_t>(nodes_.size() - 1);
}

void OrderBook::release(std::int32_t node) { free_list_.push_back(node); }

void OrderBook::addLimitOrder(const Order& order) {
    PriceLevel& level = (order.side == Side::Buy) ? bids_[order.price] : asks_[order.price];

    const std::int32_t idx = allocate(order);
    if (level.tail == kNil) {
        level.head = idx;
    } else {
        nodes_[static_cast<std::size_t>(level.tail)].next = idx;
        nodes_[static_cast<std::size_t>(idx)].prev = level.tail;
    }
    level.tail = idx;
    level.total_quantity += order.quantity;

    locations_.emplace(order.id, OrderLocation{order.side, order.price, &level, idx});
}

bool OrderBook::cancelOrder(OrderId id) {
    auto loc_it = locations_.find(id);
    if (loc_it == locations_.end()) {
        return false;
    }
    const OrderLocation& loc = loc_it->second;

    PriceLevel& level = *loc.level;
    Node& node = nodes_[static_cast<std::size_t>(loc.node)];
    level.total_quantity -= node.quantity;

    if (node.prev != kNil) {
        nodes_[static_cast<std::size_t>(node.prev)].next = node.next;
    } else {
        level.head = node.next;
    }
    if (node.next != kNil) {
        nodes_[static_cast<std::size_t>(node.next)].prev = node.prev;
    } else {
        level.tail = node.prev;
    }
    release(loc.node);

    if (level.head == kNil) {
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
    Node& node = nodes_[static_cast<std::size_t>(loc.node)];
    level.total_quantity = level.total_quantity - node.quantity + new_quantity;
    node.quantity = new_quantity;
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
std::vector<OrderId> OrderBook::ordersAtIn(const Levels& levels, Price price) const {
    std::vector<OrderId> ids;
    auto it = levels.find(price);
    if (it == levels.end()) {
        return ids;
    }
    for (std::int32_t idx = it->second.head; idx != kNil;
         idx = nodes_[static_cast<std::size_t>(idx)].next) {
        ids.push_back(nodes_[static_cast<std::size_t>(idx)].id);
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
