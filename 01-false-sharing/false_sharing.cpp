// False sharing benchmark
//
// Each thread increments its OWN counter. The threads are logically
// independent, but when counters share a 64-byte cache line, cores keep
// stealing that line from each other (cache coherence traffic).
//
// Variants:
//   packed / padded            - plain load+store increment
//   local accumulate           - accumulate in a register, publish once
//   packed / padded fetch_add  - atomic read-modify-write (lock add on x86)
//
// Build:  g++ -std=c++20 -O2 -pthread false_sharing.cpp -o false_sharing
// Run:    ./false_sharing [threads] [iterations_per_thread]
 
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>
 
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif
 
constexpr std::size_t kCacheLine = 64;
 
// Logical CPU stride between threads. With SMT (Hyper-Threading) logical
// CPUs 2k and 2k+1 are usually siblings on one physical core, so stride 2
// puts each thread on its own physical core. Check `lscpu -e` on Linux.
constexpr unsigned kCpuStride = 2;
 
// Counters placed back to back: 8 of them fit into one cache line.
struct Packed {
    std::atomic<std::uint64_t> value{0};
};
 
// Each counter occupies its own cache line.
struct alignas(kCacheLine) Padded {
    std::atomic<std::uint64_t> value{0};
};
 
static_assert(sizeof(Packed) == 8);
static_assert(sizeof(Padded) == kCacheLine);
 
void pin_self(unsigned cpu) {
#if defined(_WIN32)
    SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << cpu);
#elif defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
#else
    (void)cpu;  // no pinning on other platforms
#endif
}
 
// Plain increment: each counter has a single writer, so load+store is
// correct and compiles to ordinary movs (no lock prefix).
template <typename Counter>
void work_store(Counter& c, std::uint64_t iters) {
    for (std::uint64_t i = 0; i < iters; ++i) {
        c.value.store(c.value.load(std::memory_order_relaxed) + 1,
                      std::memory_order_relaxed);
    }
}
 
// Atomic read-modify-write: needs exclusive ownership of the line
// on every single increment.
template <typename Counter>
void work_rmw(Counter& c, std::uint64_t iters) {
    for (std::uint64_t i = 0; i < iters; ++i) {
        c.value.fetch_add(1, std::memory_order_relaxed);
    }
}
 
// The practical fix: accumulate locally, write shared memory once.
template <typename Counter>
void work_local(Counter& c, std::uint64_t iters) {
    std::uint64_t local = 0;
    for (std::uint64_t i = 0; i < iters; ++i) {
        local += 1;
        asm volatile("" : "+r"(local));  // keep the compiler from folding the loop
    }
    c.value.store(local, std::memory_order_relaxed);
}
 
template <typename Counter, typename Fn>
double run_ms(unsigned threads, std::uint64_t iters, Fn fn) {
    std::vector<Counter> counters(threads);
    std::vector<std::thread> pool;
    std::atomic<bool> go{false};
 
    for (unsigned t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] {
            pin_self(t * kCpuStride);
            while (!go.load(std::memory_order_acquire)) {}  // start together
            fn(counters[t], iters);
        });
    }
 
    auto start = std::chrono::steady_clock::now();
    go.store(true, std::memory_order_release);
    for (auto& th : pool) th.join();
    auto end = std::chrono::steady_clock::now();
 
    for (auto& c : counters) {
        if (c.value.load() != iters) {
            std::puts("check failed");
            std::exit(1);
        }
    }
    return std::chrono::duration<double, std::milli>(end - start).count();
}
 
template <typename Counter, typename Fn>
double median_ms(unsigned threads, std::uint64_t iters, Fn fn, int runs = 7) {
    std::vector<double> r;
    for (int i = 0; i < runs; ++i) r.push_back(run_ms<Counter>(threads, iters, fn));
    std::sort(r.begin(), r.end());
    return r[r.size() / 2];
}
 
int main(int argc, char** argv) {
    unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    unsigned threads = argc > 1 ? std::atoi(argv[1]) : std::min(4u, hw / kCpuStride);
    std::uint64_t iters = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 100'000'000ULL;
 
    if (threads == 0 || (threads - 1) * kCpuStride >= hw) {
        std::printf("threads must be in 1..%u for this machine\n", (hw + kCpuStride - 1) / kCpuStride);
        return 1;
    }
 
    std::printf("threads=%u  iters/thread=%llu  hw_threads=%u\n\n",
                threads, (unsigned long long)iters, hw);
 
    auto store  = [](auto& c, auto n) { work_store(c, n); };
    auto rmw    = [](auto& c, auto n) { work_rmw(c, n); };
    auto local  = [](auto& c, auto n) { work_local(c, n); };
 
    double packed_st  = median_ms<Packed>(threads, iters, store);
    double padded_st  = median_ms<Padded>(threads, iters, store);
    double local_acc  = median_ms<Padded>(threads, iters, local);
    double packed_rmw = median_ms<Packed>(threads, iters, rmw);
    double padded_rmw = median_ms<Padded>(threads, iters, rmw);
 
    auto row = [&](const char* name, double ms, double baseline) {
        std::printf("%-24s %10.1f %10.3f %9.1fx\n", name, ms, ms * 1e6 / double(iters), baseline / ms);
    };
 
    std::printf("%-24s %10s %10s %10s\n", "variant", "ms", "ns/op", "speedup");
    row("packed store",      packed_st,  packed_st);
    row("padded store",      padded_st,  packed_st);
    row("local accumulate",  local_acc,  packed_st);
    std::puts("");
    row("packed fetch_add",  packed_rmw, packed_rmw);
    row("padded fetch_add",  padded_rmw, packed_rmw);
}