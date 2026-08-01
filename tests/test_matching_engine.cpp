#include "orderbook/matching_engine.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace orderbook;

TEST_CASE("an order that doesn't cross rests", "[matching]") {
    MatchingEngine engine;
    std::vector<Trade> trades;
    REQUIRE(engine.submit({.id = 1, .side = Side::Sell, .price = 101, .quantity = 10}, trades));
    REQUIRE(engine.submit({.id = 2, .side = Side::Buy, .price = 100, .quantity = 10}, trades));

    REQUIRE(trades.empty());
    REQUIRE(engine.book().bestBid() == 100);
    REQUIRE(engine.book().bestAsk() == 101);
}

TEST_CASE("a crossing order trades at the resting price", "[matching]") {
    MatchingEngine engine;
    std::vector<Trade> trades;
    engine.submit({.id = 1, .side = Side::Sell, .price = 100, .quantity = 10}, trades);
    engine.submit({.id = 2, .side = Side::Buy, .price = 103, .quantity = 10}, trades);

    REQUIRE(trades == std::vector<Trade>{{.maker_id = 1, .taker_id = 2, .price = 100, .quantity = 10}});
    REQUIRE(engine.book().size() == 0);
    REQUIRE(engine.book().bestAsk() == std::nullopt);
    REQUIRE(engine.book().bestBid() == std::nullopt);
}

TEST_CASE("a partly filled resting order keeps its place", "[matching]") {
    MatchingEngine engine;
    std::vector<Trade> trades;
    engine.submit({.id = 1, .side = Side::Buy, .price = 100, .quantity = 10}, trades);
    engine.submit({.id = 2, .side = Side::Buy, .price = 100, .quantity = 5}, trades);
    engine.submit({.id = 3, .side = Side::Sell, .price = 100, .quantity = 4}, trades);

    REQUIRE(trades == std::vector<Trade>{{.maker_id = 1, .taker_id = 3, .price = 100, .quantity = 4}});
    REQUIRE(engine.book().ordersAt(Side::Buy, 100) == std::vector<OrderId>{1, 2});
    REQUIRE(engine.book().front(Side::Buy)->quantity == 6);
    REQUIRE(engine.book().quantityAt(Side::Buy, 100) == 11);
    REQUIRE_FALSE(engine.book().hasOrder(3));
}

TEST_CASE("orders at the same price fill oldest first", "[matching]") {
    MatchingEngine engine;
    std::vector<Trade> trades;
    engine.submit({.id = 1, .side = Side::Sell, .price = 100, .quantity = 3}, trades);
    engine.submit({.id = 2, .side = Side::Sell, .price = 100, .quantity = 3}, trades);
    engine.submit({.id = 3, .side = Side::Sell, .price = 100, .quantity = 3}, trades);
    engine.submit({.id = 4, .side = Side::Buy, .price = 100, .quantity = 7}, trades);

    REQUIRE(trades == std::vector<Trade>{{.maker_id = 1, .taker_id = 4, .price = 100, .quantity = 3},
                                         {.maker_id = 2, .taker_id = 4, .price = 100, .quantity = 3},
                                         {.maker_id = 3, .taker_id = 4, .price = 100, .quantity = 1}});
    REQUIRE(engine.book().ordersAt(Side::Sell, 100) == std::vector<OrderId>{3});
    REQUIRE(engine.book().quantityAt(Side::Sell, 100) == 2);
}

TEST_CASE("an order walks several levels and rests the rest", "[matching]") {
    MatchingEngine engine;
    std::vector<Trade> trades;
    engine.submit({.id = 1, .side = Side::Buy, .price = 102, .quantity = 5}, trades);
    engine.submit({.id = 2, .side = Side::Buy, .price = 101, .quantity = 5}, trades);
    engine.submit({.id = 3, .side = Side::Buy, .price = 99, .quantity = 5}, trades);
    engine.submit({.id = 4, .side = Side::Sell, .price = 100, .quantity = 12}, trades);

    REQUIRE(trades == std::vector<Trade>{{.maker_id = 1, .taker_id = 4, .price = 102, .quantity = 5},
                                         {.maker_id = 2, .taker_id = 4, .price = 101, .quantity = 5}});
    REQUIRE(engine.book().bestBid() == 99);
    REQUIRE(engine.book().bestAsk() == 100);
    REQUIRE(engine.book().quantityAt(Side::Sell, 100) == 2);
}

TEST_CASE("zero quantity and resting ids are rejected", "[matching]") {
    MatchingEngine engine;
    std::vector<Trade> trades;
    REQUIRE_FALSE(engine.submit({.id = 1, .side = Side::Buy, .price = 100, .quantity = 0}, trades));
    REQUIRE(engine.submit({.id = 1, .side = Side::Buy, .price = 100, .quantity = 5}, trades));
    REQUIRE_FALSE(engine.submit({.id = 1, .side = Side::Sell, .price = 100, .quantity = 5}, trades));

    REQUIRE(trades.empty());
    REQUIRE(engine.book().quantityAt(Side::Buy, 100) == 5);
}

TEST_CASE("a cancelled order doesn't trade", "[matching]") {
    MatchingEngine engine;
    std::vector<Trade> trades;
    engine.submit({.id = 1, .side = Side::Sell, .price = 100, .quantity = 5}, trades);
    engine.submit({.id = 2, .side = Side::Sell, .price = 101, .quantity = 5}, trades);
    REQUIRE(engine.cancel(1));
    REQUIRE_FALSE(engine.cancel(1));
    engine.submit({.id = 3, .side = Side::Buy, .price = 101, .quantity = 5}, trades);

    REQUIRE(trades == std::vector<Trade>{{.maker_id = 2, .taker_id = 3, .price = 101, .quantity = 5}});
    REQUIRE(engine.book().size() == 0);
}
