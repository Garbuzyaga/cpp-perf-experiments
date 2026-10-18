// Cost of memory_order on a single thread
//
// The same atomic store or load in a loop, with different memory orders.
// On x86 most of them compile to a plain mov. The exception is a seq_cst
// store: it becomes xchg (or mov + mfence), which has to wait for the
// store buffer to drain. seq_cst is what you get by default with
// x.store(v) or x = v.
//
// Build:  g++ -std=c++20 -O2 memory_order.cpp -o memory_order
// Run:    ./memory_order [iterations]
// Asm:    g++ -std=c++20 -O2 -S memory_order.cpp  (look for xchg / mfence)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

alignas(64) std::atomic<std::uint64_t> g_value{0};

template <std::memory_order Order>
void store_loop(std::uint64_t iters) {
    for (std::uint64_t i = 0; i < iters; ++i) {
        g_value.store(i, Order);
    }
}

template <std::memory_order Order>
void load_loop(std::uint64_t iters) {
    std::uint64_t sum = 0;
    for (std::uint64_t i = 0; i < iters; ++i) {
        sum += g_value.load(Order);
    }
    asm volatile("" : : "r"(sum));  // keep the loads alive
}

void plain_assign_loop(std::uint64_t iters) {
    for (std::uint64_t i = 0; i < iters; ++i) {
        g_value = i;  // same as store(i, seq_cst)
    }
}

void release_plus_fence_loop(std::uint64_t iters) {
    for (std::uint64_t i = 0; i < iters; ++i) {
        g_value.store(i, std::memory_order_release);
        std::atomic_thread_fence(std::memory_order_seq_cst);
    }
}

template <typename Fn>
double median_ns_per_op(Fn fn, std::uint64_t iters, int runs = 7) {
    std::vector<double> r;
    for (int i = 0; i < runs; ++i) {
        auto start = std::chrono::steady_clock::now();
        fn(iters);
        auto end = std::chrono::steady_clock::now();
        r.push_back(std::chrono::duration<double, std::nano>(end - start).count() / double(iters));
    }
    std::sort(r.begin(), r.end());
    return r[r.size() / 2];
}

int main(int argc, char** argv) {
    std::uint64_t iters = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 100'000'000ULL;

    struct Case { const char* name; void (*fn)(std::uint64_t); };
    const Case cases[] = {
        {"store relaxed",          store_loop<std::memory_order_relaxed>},
        {"store release",          store_loop<std::memory_order_release>},
        {"store seq_cst",          store_loop<std::memory_order_seq_cst>},
        {"x = v (default)",        plain_assign_loop},
        {"store release + fence",  release_plus_fence_loop},
        {"load relaxed",           load_loop<std::memory_order_relaxed>},
        {"load acquire",           load_loop<std::memory_order_acquire>},
        {"load seq_cst",           load_loop<std::memory_order_seq_cst>},
    };

    std::printf("iterations=%llu\n\n", (unsigned long long)iters);
    std::printf("%-24s %10s\n", "variant", "ns/op");

    double baseline = 0;
    for (const auto& c : cases) {
        double ns = median_ns_per_op(c.fn, iters);
        if (baseline == 0) baseline = ns;
        std::printf("%-24s %10.3f   %5.1fx\n", c.name, ns, ns / baseline);
    }
}