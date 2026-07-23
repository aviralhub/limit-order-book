#include "orderbook/order_book.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <random>
#include <vector>

using namespace orderbook;

namespace {

// Same interface as OrderBook, done the slow obvious way.
struct Model {
    std::vector<Order> orders; // in arrival order

    bool add(const Order& order) {
        if (find(order.id) != orders.end()) {
            return false;
        }
        orders.push_back(order);
        return true;
    }

    bool cancel(OrderId id) {
        auto it = find(id);
        if (it == orders.end()) {
            return false;
        }
        orders.erase(it);
        return true;
    }

    bool modify(OrderId id, Quantity quantity) {
        auto it = find(id);
        if (it == orders.end()) {
            return false;
        }
        it->quantity = quantity;
        return true;
    }

    std::optional<Price> best(Side side) const {
        std::optional<Price> best;
        for (const Order& o : orders) {
            if (o.side == side && (!best || (side == Side::Buy ? o.price > *best : o.price < *best))) {
                best = o.price;
            }
        }
        return best;
    }

    Quantity quantityAt(Side side, Price price) const {
        Quantity total = 0;
        for (const Order& o : orders) {
            if (o.side == side && o.price == price) {
                total += o.quantity;
            }
        }
        return total;
    }

    std::vector<OrderId> ordersAt(Side side, Price price) const {
        std::vector<OrderId> ids;
        for (const Order& o : orders) {
            if (o.side == side && o.price == price) {
                ids.push_back(o.id);
            }
        }
        return ids;
    }

    std::vector<Order>::iterator find(OrderId id) {
        return std::find_if(orders.begin(), orders.end(), [id](const Order& o) { return o.id == id; });
    }
};

} // namespace

TEST_CASE("random adds, cancels and modifies match a simple model", "[randomised]") {
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> op(0, 9);
    // small ranges so ids collide and levels fill up and empty often
    std::uniform_int_distribution<OrderId> id(1, 300);
    std::uniform_int_distribution<Price> price(95, 105);
    std::uniform_int_distribution<Quantity> quantity(1, 50);
    std::uniform_int_distribution<int> side(0, 1);

    OrderBook book(16);
    Model model;

    for (int i = 0; i < 20000; ++i) {
        const int roll = op(rng);
        if (roll < 5) {
            Order o{id(rng), side(rng) == 0 ? Side::Buy : Side::Sell, price(rng), quantity(rng)};
            REQUIRE(book.addLimitOrder(o) == model.add(o));
        } else if (roll < 8) {
            OrderId target = id(rng);
            REQUIRE(book.cancelOrder(target) == model.cancel(target));
        } else {
            OrderId target = id(rng);
            Quantity q = quantity(rng);
            REQUIRE(book.modifyOrder(target, q) == model.modify(target, q));
        }

        REQUIRE(book.size() == model.orders.size());
        REQUIRE(book.bestBid() == model.best(Side::Buy));
        REQUIRE(book.bestAsk() == model.best(Side::Sell));

        const Price p = price(rng);
        const Side s = side(rng) == 0 ? Side::Buy : Side::Sell;
        REQUIRE(book.quantityAt(s, p) == model.quantityAt(s, p));
        REQUIRE(book.ordersAt(s, p) == model.ordersAt(s, p));
    }
}
