#pragma once

#include "orderbook/order.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace orderbook {

// Resting orders only. MatchingEngine does the crossing.
class OrderBook {
public:
    explicit OrderBook(std::size_t capacity = 1 << 16);

    // false for a zero quantity or an id that's already resting
    bool addLimitOrder(const Order& order);

    // false if the id isn't in the book
    bool cancelOrder(OrderId id);

    // keeps the order's place in the queue, a quantity of 0 cancels it
    bool modifyOrder(OrderId id, Quantity new_quantity);

    std::optional<Price> bestBid() const;
    std::optional<Price> bestAsk() const;

    Quantity quantityAt(Side side, Price price) const;

    // oldest first
    std::vector<OrderId> ordersAt(Side side, Price price) const;

    // oldest order at the best price
    std::optional<Order> front(Side side) const;

    bool hasOrder(OrderId id) const { return locations_.contains(id); }
    std::size_t size() const { return locations_.size(); }

private:
    static constexpr std::int32_t kNil = -1;

    // Links are indices into nodes_, not pointers, so the pool can grow.
    // Price and side aren't stored here, the level and location already have them.
    struct Node {
        OrderId id;
        Quantity quantity;
        std::int32_t next = kNil;
        std::int32_t prev = kNil;
    };

    struct PriceLevel {
        Quantity total_quantity = 0;
        std::int32_t head = kNil;
        std::int32_t tail = kNil;
    };

    // std::map doesn't move its nodes and a level is only erased once it's
    // empty, so `level` can't dangle. side and price are only used for that erase.
    struct OrderLocation {
        Side side;
        Price price;
        PriceLevel* level;
        std::int32_t node;
    };

    // begin() is the best price on both sides
    using Bids = std::map<Price, PriceLevel, std::greater<Price>>;
    using Asks = std::map<Price, PriceLevel, std::less<Price>>;

    std::int32_t allocate(const Order& order);
    void release(std::int32_t node);

    Bids bids_;
    Asks asks_;
    std::unordered_map<OrderId, OrderLocation> locations_;

    std::vector<Node> nodes_;
    std::vector<std::int32_t> free_list_;

    template <typename Levels>
    static std::optional<Price> bestPrice(const Levels& levels);

    template <typename Levels>
    static Quantity quantityAtIn(const Levels& levels, Price price);

    template <typename Levels>
    std::vector<OrderId> ordersAtIn(const Levels& levels, Price price) const;

    template <typename Levels>
    std::optional<Order> frontIn(const Levels& levels, Side side) const;
};

} // namespace orderbook
