# Design notes

## Prices

Prices are `int64_t` ticks. Matching compares prices with `==` and `<=`, and with doubles two prices
that should be equal can come out different depending on how they were computed. Converting to a
decimal only matters for display, and there's none of that here.

## std::map per side

Best bid and best ask get asked for more than anything else. With the levels in a `std::map` the
best price is `begin()`, so it's a couple of pointer hops. An `unordered_map` would need a scan or a
separate heap kept in sync. Insert is O(log P) instead of O(1), which is fine.

Bids are sorted with `std::greater` and asks with `std::less`, so `begin()` is the best price on
both sides and the lookups are one template each instead of two copies.

## Finding an order

`locations_` maps an order id to where the order is. Without it cancel would have to search every
level.

It started as side, price and a `std::list` iterator, so cancel and modify still had to find the
level in the map by price before doing their O(1) work. Now it also holds a `PriceLevel*`. That's
safe because `std::map` never moves its nodes, and a level is only erased when it's empty, so no
order can still point at it. The map is only touched when a cancel empties a level.

Modify went from about 750 ns to 9 ns and cancel from about 750 ns to under 200 ns. Add got about
20% slower because `OrderLocation` went from 24 to 32 bytes.
