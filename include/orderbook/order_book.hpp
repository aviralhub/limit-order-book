#pragma once

#include "orderbook/order.hpp"

#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace orderbook {

// Resting orders only, nothing is matched here.
class OrderBook {
public:
    void addLimitOrder(const Order& order);

    // false if the id isn't in the book
    bool cancelOrder(OrderId id);

    // keeps the order's place in the queue
    bool modifyOrder(OrderId id, Quantity new_quantity);

    std::optional<Price> bestBid() const;
    std::optional<Price> bestAsk() const;

    Quantity quantityAt(Side side, Price price) const;

    // oldest first
    std::vector<OrderId> ordersAt(Side side, Price price) const;

    bool hasOrder(OrderId id) const { return locations_.contains(id); }
    std::size_t size() const { return locations_.size(); }

private:
    struct PriceLevel {
        Quantity total_quantity = 0;
        std::list<Order> orders;
    };

    struct OrderLocation {
        Side side;
        Price price;
        std::list<Order>::iterator it;
    };

    // begin() is the best price on both sides
    using Bids = std::map<Price, PriceLevel, std::greater<Price>>;
    using Asks = std::map<Price, PriceLevel, std::less<Price>>;

    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, OrderLocation> locations_;

    template <typename Levels>
    static std::optional<Price> bestPrice(const Levels& levels);

    template <typename Levels>
    static Quantity quantityAtIn(const Levels& levels, Price price);

    template <typename Levels>
    static std::vector<OrderId> ordersAtIn(const Levels& levels, Price price);
};

} // namespace orderbook
