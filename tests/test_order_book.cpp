#include "orderbook/order_book.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace orderbook;

TEST_CASE("empty book has no best bid or ask", "[order_book]") {
    OrderBook book;
    REQUIRE(book.bestBid() == std::nullopt);
    REQUIRE(book.bestAsk() == std::nullopt);
}

TEST_CASE("best bid is the highest buy price, best ask is the lowest sell price", "[order_book]") {
    OrderBook book;
    book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10});
    book.addLimitOrder({.id = 2, .side = Side::Buy, .price = 105, .quantity = 5});
    book.addLimitOrder({.id = 3, .side = Side::Sell, .price = 110, .quantity = 8});
    book.addLimitOrder({.id = 4, .side = Side::Sell, .price = 108, .quantity = 3});

    REQUIRE(book.bestBid() == 105);
    REQUIRE(book.bestAsk() == 108);
}

TEST_CASE("quantity at a price level sums all resting orders there", "[order_book]") {
    OrderBook book;
    book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10});
    book.addLimitOrder({.id = 2, .side = Side::Buy, .price = 100, .quantity = 7});

    REQUIRE(book.quantityAt(Side::Buy, 100) == 17);
}

TEST_CASE("orders at a price level are returned in FIFO (price-time priority) order", "[order_book]") {
    OrderBook book;
    book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10});
    book.addLimitOrder({.id = 2, .side = Side::Buy, .price = 100, .quantity = 7});
    book.addLimitOrder({.id = 3, .side = Side::Buy, .price = 100, .quantity = 3});

    REQUIRE(book.ordersAt(Side::Buy, 100) == std::vector<OrderId>{1, 2, 3});
}

TEST_CASE("cancelling an order removes it and updates level quantity", "[order_book]") {
    OrderBook book;
    book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10});
    book.addLimitOrder({.id = 2, .side = Side::Buy, .price = 100, .quantity = 7});

    REQUIRE(book.cancelOrder(1));
    REQUIRE_FALSE(book.hasOrder(1));
    REQUIRE(book.quantityAt(Side::Buy, 100) == 7);
    REQUIRE(book.ordersAt(Side::Buy, 100) == std::vector<OrderId>{2});
}

TEST_CASE("cancelling the last order at a price level removes the level", "[order_book]") {
    OrderBook book;
    book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10});

    REQUIRE(book.cancelOrder(1));
    REQUIRE(book.bestBid() == std::nullopt);
    REQUIRE(book.quantityAt(Side::Buy, 100) == 0);
}

TEST_CASE("cancelling an unknown order id fails cleanly", "[order_book]") {
    OrderBook book;
    REQUIRE_FALSE(book.cancelOrder(999));
}

TEST_CASE("modifying an order's quantity updates the level total", "[order_book]") {
    OrderBook book;
    book.addLimitOrder({.id = 1, .side = Side::Sell, .price = 100, .quantity = 10});
    book.addLimitOrder({.id = 2, .side = Side::Sell, .price = 100, .quantity = 5});

    REQUIRE(book.modifyOrder(1, 3));
    REQUIRE(book.quantityAt(Side::Sell, 100) == 8);
}

TEST_CASE("zero quantity never rests", "[order_book]") {
    OrderBook book;
    REQUIRE_FALSE(book.addLimitOrder({.id = 3, .side = Side::Buy, .price = 100, .quantity = 0}));
    book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10});
    book.addLimitOrder({.id = 2, .side = Side::Buy, .price = 101, .quantity = 5});

    REQUIRE(book.modifyOrder(2, 0));
    REQUIRE_FALSE(book.hasOrder(2));
    REQUIRE(book.size() == 1);
    REQUIRE(book.bestBid() == 100);
    REQUIRE(book.quantityAt(Side::Buy, 101) == 0);
}

TEST_CASE("modifying an unknown order id fails cleanly", "[order_book]") {
    OrderBook book;
    REQUIRE_FALSE(book.modifyOrder(999, 5));
}

TEST_CASE("modifying quantity does not move the order in its queue", "[order_book]") {
    OrderBook book;
    book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10});
    book.addLimitOrder({.id = 2, .side = Side::Buy, .price = 100, .quantity = 7});
    book.addLimitOrder({.id = 3, .side = Side::Buy, .price = 100, .quantity = 3});

    REQUIRE(book.modifyOrder(1, 1));
    REQUIRE(book.ordersAt(Side::Buy, 100) == std::vector<OrderId>{1, 2, 3});

    REQUIRE(book.modifyOrder(2, 99));
    REQUIRE(book.ordersAt(Side::Buy, 100) == std::vector<OrderId>{1, 2, 3});
    REQUIRE(book.quantityAt(Side::Buy, 100) == 103);
}

TEST_CASE("stored level stays valid as other levels are added and erased", "[order_book]") {
    OrderBook book;
    book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10});

    // Churn other levels around the one order 1 rests in.
    for (OrderId id = 2; id < 60; ++id) {
        book.addLimitOrder({.id = id, .side = Side::Buy, .price = 100 + static_cast<Price>(id), .quantity = 5});
    }
    for (OrderId id = 2; id < 60; ++id) {
        REQUIRE(book.cancelOrder(id));
    }

    REQUIRE(book.modifyOrder(1, 42));
    REQUIRE(book.quantityAt(Side::Buy, 100) == 42);
    REQUIRE(book.cancelOrder(1));
    REQUIRE(book.size() == 0);
    REQUIRE(book.bestBid() == std::nullopt);
}

TEST_CASE("adding a duplicate order id is rejected and changes nothing", "[order_book]") {
    OrderBook book;
    REQUIRE(book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10}));
    REQUIRE_FALSE(book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 7}));

    REQUIRE(book.size() == 1);
    REQUIRE(book.quantityAt(Side::Buy, 100) == 10);
    REQUIRE(book.ordersAt(Side::Buy, 100) == std::vector<OrderId>{1});

    REQUIRE(book.cancelOrder(1));
    REQUIRE(book.size() == 0);
    REQUIRE(book.quantityAt(Side::Buy, 100) == 0);
    REQUIRE(book.bestBid() == std::nullopt);
}

TEST_CASE("bids and asks on the same price level do not interfere", "[order_book]") {
    OrderBook book;
    book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10});
    book.addLimitOrder({.id = 2, .side = Side::Sell, .price = 100, .quantity = 4});

    REQUIRE(book.quantityAt(Side::Buy, 100) == 10);
    REQUIRE(book.quantityAt(Side::Sell, 100) == 4);
}

TEST_CASE("size tracks the number of resting orders", "[order_book]") {
    OrderBook book;
    REQUIRE(book.size() == 0);
    book.addLimitOrder({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10});
    book.addLimitOrder({.id = 2, .side = Side::Sell, .price = 105, .quantity = 4});
    REQUIRE(book.size() == 2);
    book.cancelOrder(1);
    REQUIRE(book.size() == 1);
}
