#include "orderbook/order_book.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>
#include <x86intrin.h>

namespace {

using orderbook::Order;
using orderbook::OrderBook;
using orderbook::OrderId;
using orderbook::Price;
using orderbook::Quantity;
using orderbook::Side;

using Clock = std::chrono::steady_clock;

constexpr std::size_t kRestingOrders = 100'000;
constexpr std::size_t kWarmupOps = 100'000;
constexpr std::size_t kMeasuredOps = 1'000'000;
constexpr std::uint32_t kSeed = 12345;

constexpr double kMeanPrice = 30000.0;
constexpr double kPriceStdDev = 5000.0;
constexpr Quantity kMinQuantity = 1;
constexpr Quantity kMaxQuantity = 500;

// equal adds and cancels so the book stays the same depth through the run
constexpr int kAddWeight = 45;
constexpr int kCancelWeight = 45;
constexpr int kModifyWeight = 10;

enum class OpKind { Add, Cancel, Modify };

struct Op {
    OpKind kind;
    Order order;
    OrderId target;
    Quantity new_quantity;
};

std::int64_t percentile(const std::vector<std::int64_t>& sorted, double fraction) {
    if (sorted.empty()) {
        return 0;
    }
    std::size_t index = static_cast<std::size_t>(fraction * static_cast<double>(sorted.size()));
    return sorted[std::min(index, sorted.size() - 1)];
}

void report(const char* label, std::vector<std::int64_t>& samples) {
    if (samples.empty()) {
        return;
    }
    std::sort(samples.begin(), samples.end());
    std::printf("%-8s %9zu %7lld %7lld %7lld %9lld\n", label, samples.size(),
                static_cast<long long>(percentile(samples, 0.50)),
                static_cast<long long>(percentile(samples, 0.99)),
                static_cast<long long>(percentile(samples, 0.999)),
                static_cast<long long>(samples.back()));
}

void histogram(const std::vector<std::int64_t>& sorted) {
    const std::int64_t bounds[] = {50, 100, 250, 500, 1000, 5000};
    const char* labels[] = {"<50", "50-100", "100-250", "250-500", "500-1000", "1000-5000", "5000+"};
    constexpr std::size_t kBuckets = std::size(labels);

    std::size_t counts[kBuckets] = {};
    for (std::int64_t v : sorted) {
        std::size_t b = kBuckets - 1;
        for (std::size_t i = 0; i + 1 < kBuckets; ++i) {
            if (v < bounds[i]) {
                b = i;
                break;
            }
        }
        ++counts[b];
    }

    std::printf("\nns         count  share\n");
    for (std::size_t i = 0; i < kBuckets; ++i) {
        double share = 100.0 * static_cast<double>(counts[i]) / static_cast<double>(sorted.size());
        std::printf("%-9s %7zu  %5.1f%%\n", labels[i], counts[i], share);
    }
}

// steady_clock only ticks every 100ns here, too coarse for these operations
inline std::uint64_t tsc_now() { return __rdtsc(); }

// cycles per ns
double calibrate_tsc() {
    const auto wall_start = Clock::now();
    const std::uint64_t tsc_start = tsc_now();
    // busy wait, sleeping lets the core clock down
    while (Clock::now() - wall_start < std::chrono::milliseconds(200)) {
    }
    const std::uint64_t tsc_end = tsc_now();
    const auto wall_end = Clock::now();

    const auto elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(wall_end - wall_start).count();
    return static_cast<double>(tsc_end - tsc_start) / static_cast<double>(elapsed_ns);
}

// cost of two back to back reads, reported but not subtracted
std::uint64_t tsc_overhead() {
    std::vector<std::int64_t> samples;
    for (int i = 0; i < 200'000; ++i) {
        std::uint64_t a = tsc_now();
        std::uint64_t b = tsc_now();
        samples.push_back(static_cast<std::int64_t>(b - a));
    }
    std::sort(samples.begin(), samples.end());
    return static_cast<std::uint64_t>(percentile(samples, 0.50));
}

Order random_order(std::mt19937& rng, OrderId id) {
    std::normal_distribution<double> price(kMeanPrice, kPriceStdDev);
    std::uniform_int_distribution<Quantity> quantity(kMinQuantity, kMaxQuantity);
    std::uniform_int_distribution<int> side(0, 1);
    return Order{id, side(rng) == 0 ? Side::Buy : Side::Sell, static_cast<Price>(price(rng)),
                 quantity(rng)};
}

// Built up front so the RNG isn't inside the timed loop. `live` tracks resting
// ids so cancels and modifies always hit a real order.
std::vector<Op> build_plan(std::size_t count, std::mt19937& rng, std::vector<OrderId>& live,
                           OrderId& next_id) {
    std::uniform_int_distribution<int> mix(1, kAddWeight + kCancelWeight + kModifyWeight);
    std::uniform_int_distribution<Quantity> quantity(kMinQuantity, kMaxQuantity);

    std::vector<Op> plan;
    plan.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        int roll = mix(rng);
        if (roll <= kAddWeight || live.empty()) {
            Order order = random_order(rng, next_id++);
            plan.push_back(Op{OpKind::Add, order, 0, 0});
            live.push_back(order.id);
        } else if (roll <= kAddWeight + kCancelWeight) {
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            std::size_t slot = pick(rng);
            OrderId id = live[slot];
            live[slot] = live.back();
            live.pop_back();
            plan.push_back(Op{OpKind::Cancel, Order{}, id, 0});
        } else {
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            plan.push_back(Op{OpKind::Modify, Order{}, live[pick(rng)], quantity(rng)});
        }
    }
    return plan;
}

void run(OrderBook& book, const Op& op) {
    switch (op.kind) {
        case OpKind::Add: book.addLimitOrder(op.order); break;
        case OpKind::Cancel: book.cancelOrder(op.target); break;
        case OpKind::Modify: book.modifyOrder(op.target, op.new_quantity); break;
    }
}

} // namespace

int main() {
    const double cycles_per_ns = calibrate_tsc();
    const std::uint64_t overhead = tsc_overhead();

    std::mt19937 rng(kSeed);
    std::vector<OrderId> live;
    OrderId next_id = 1;
    OrderBook book;

    for (std::size_t i = 0; i < kRestingOrders; ++i) {
        Order order = random_order(rng, next_id++);
        book.addLimitOrder(order);
        live.push_back(order.id);
    }
    for (const Op& op : build_plan(kWarmupOps, rng, live, next_id)) {
        run(book, op);
    }

    const std::vector<Op> plan = build_plan(kMeasuredOps, rng, live, next_id);
    std::vector<std::int64_t> samples[3];
    for (auto& s : samples) {
        s.reserve(kMeasuredOps);
    }

    // the switch stays outside the timed region
    const auto wall_start = Clock::now();
    for (const Op& op : plan) {
        std::uint64_t t0 = 0;
        std::uint64_t t1 = 0;
        switch (op.kind) {
            case OpKind::Add:
                t0 = tsc_now();
                book.addLimitOrder(op.order);
                t1 = tsc_now();
                break;
            case OpKind::Cancel:
                t0 = tsc_now();
                book.cancelOrder(op.target);
                t1 = tsc_now();
                break;
            case OpKind::Modify:
                t0 = tsc_now();
                book.modifyOrder(op.target, op.new_quantity);
                t1 = tsc_now();
                break;
        }
        samples[static_cast<int>(op.kind)].push_back(static_cast<std::int64_t>(t1 - t0));
    }
    const auto wall_end = Clock::now();

    std::vector<std::int64_t> all;
    for (auto& s : samples) {
        for (std::int64_t& v : s) {
            v = static_cast<std::int64_t>(static_cast<double>(v) / cycles_per_ns);
        }
        all.insert(all.end(), s.begin(), s.end());
    }

    const double seconds = std::chrono::duration<double>(wall_end - wall_start).count();
    std::printf("%zu resting orders, %zu measured ops (%d%% add, %d%% cancel, %d%% modify)\n",
                kRestingOrders, kMeasuredOps, kAddWeight, kCancelWeight, kModifyWeight);
    std::printf("rdtsc %.3f cycles/ns, timer overhead %llu cycles (included)\n", cycles_per_ns,
                static_cast<unsigned long long>(overhead));
    std::printf("%.0f ops/sec, %zu orders left\n\n", static_cast<double>(kMeasuredOps) / seconds,
                book.size());

    std::printf("op           count     p50     p99   p99.9       max  (ns)\n");
    report("add", samples[0]);
    report("cancel", samples[1]);
    report("modify", samples[2]);
    report("all", all);
    histogram(all);
}
