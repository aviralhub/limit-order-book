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

Modify went from about 600 ns to 9 ns and cancel from about 650 ns to under 100 ns. Add got about
15% slower because `OrderLocation` went from 24 to 32 bytes.

## Order pool

Each level used to be a `std::list<Order>`. Its iterators stay valid when other elements are added or
removed, which is what `OrderLocation` needed, but it costs an allocation on every add and a free on
every cancel.

Now all orders are in one `std::vector<Node>` and each level's queue goes through `next`/`prev`
fields. Those are `int32_t` indices, not pointers, so the vector can reallocate without breaking
anything. Cancelled slots go on a free list and get reused.

Add got faster and p99.9 dropped to about half. Cancel got slower than it was right before this
change and p50 only improved about 10%, so this is mostly a tail latency change.

`Node` is only id, quantity and the two links, 24 bytes. Price is already known from the level and
side from the location. Putting a whole `Order` in it would make it 40 bytes, and a cancel reads
three nodes that are usually far apart.

## Duplicate ids

`addLimitOrder` used to link the order into its level first and insert into `locations_` after. If
the id was already resting, the insert did nothing and the new order was left in the queue with no
id pointing at it. `size()` was then wrong, and cancelling the original id left a level that still
had depth and still showed up as the best bid. A randomised test against a simple model found it.

The index insert now happens first and `addLimitOrder` returns false for a duplicate. `try_emplace`
tells you whether the key was new from the same lookup, so the check doesn't cost an extra probe.

## Modify

`modifyOrder` changes the quantity in place and the order keeps its place in the queue. That's what
exchanges do for a decrease. For an increase most of them send the order to the back, since
otherwise you could hold a spot in the queue with a small order and grow it later. Here it keeps its
place either way.

A modify to 0 used to leave an order with no quantity sitting in the queue and counted by `size()`.
It's a cancel now, and adding an order with quantity 0 is rejected.

Level totals are `uint32_t` like order quantities, so a single level holding more than about 4
billion would wrap.

## Matching

`MatchingEngine` sits on top of `OrderBook` and only uses its public functions, so the book itself
stays a plain data structure and its benchmark still measures the same thing.

`submit` looks at `front()` of the other side, the oldest order at the best price. While that
crosses, it trades the smaller of the two quantities at the resting order's price. A resting order
that fills completely is cancelled out of the book. One that fills partly goes through
`modifyOrder`, which keeps its place at the front of the queue. The loop reads `front()` again every
time, so it never holds on to a level that a cancel might have just erased.

Things it doesn't do: market orders (a limit at an extreme price does the same job), self-trade
prevention, since orders don't have an owner, and remembering ids that have fully filled. The
duplicate check only covers orders that are resting.
