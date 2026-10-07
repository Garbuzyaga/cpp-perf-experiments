// Same two cores, different addresses
//
// Ping-pongs between two fixed CPUs through each of N consecutive cache
// lines in one buffer. If latency depends on which L3 slice the line's
// address maps to, it will vary from line to line even though the cores
// stay the same.
//
// Build:  g++ -std=c++20 -O2 -pthread address_latency.cpp -o address_latency
// Run:    ./address_latency [cpu_a] [cpu_b] [lines] [round_trips]
// Output: per-line latency on stdout + address_latency.csv
 
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
 
void pin_self(unsigned cpu) {
#if defined(_WIN32)
    SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << cpu);
#elif defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
#else
    (void)cpu;
#endif
}
 
struct alignas(64) Line {
    std::atomic<std::uint64_t> value{0};
};
 
double one_way_ns(Line& line, unsigned cpu_a, unsigned cpu_b, std::uint64_t round_trips) {
    line.value.store(0);
    std::atomic<int> ready{0};
    double result = 0;
 
    std::thread pong([&] {
        pin_self(cpu_b);
        ready.fetch_add(1);
        for (std::uint64_t k = 0; k < round_trips; ++k) {
            const std::uint64_t ping = 2 * k + 1;
            while (line.value.load(std::memory_order_acquire) != ping) {}
            line.value.store(ping + 1, std::memory_order_release);
        }
    });
 
    std::thread ping([&] {
        pin_self(cpu_a);
        ready.fetch_add(1);
        while (ready.load() < 2) {}
 
        auto start = std::chrono::steady_clock::now();
        for (std::uint64_t k = 0; k < round_trips; ++k) {
            line.value.store(2 * k + 1, std::memory_order_release);
            while (line.value.load(std::memory_order_acquire) != 2 * k + 2) {}
        }
        auto end = std::chrono::steady_clock::now();
 
        double ns = std::chrono::duration<double, std::nano>(end - start).count();
        result = ns / double(round_trips * 2);
    });
 
    ping.join();
    pong.join();
    return result;
}
 
int main(int argc, char** argv) {
    unsigned cpu_a = argc > 1 ? std::atoi(argv[1]) : 0;
    unsigned cpu_b = argc > 2 ? std::atoi(argv[2]) : 2;
    unsigned lines = argc > 3 ? std::atoi(argv[3]) : 64;
    std::uint64_t round_trips = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 100'000ULL;
 
    std::vector<Line> buf(lines);
 
    std::printf("cpu %u <-> cpu %u, %u lines, round_trips=%llu\n\n",
                cpu_a, cpu_b, lines, (unsigned long long)round_trips);
 
    std::vector<double> ns(lines);
    for (unsigned i = 0; i < lines; ++i) {
        std::vector<double> r;
        for (int run = 0; run < 5; ++run) r.push_back(one_way_ns(buf[i], cpu_a, cpu_b, round_trips));
        std::sort(r.begin(), r.end());
        ns[i] = r[2];
        std::printf("line %3u  %p  %5.1f ns\n", i, (void*)&buf[i], ns[i]);
    }
 
    auto [lo, hi] = std::minmax_element(ns.begin(), ns.end());
    std::printf("\nmin %.1f ns, max %.1f ns\n", *lo, *hi);
 
    if (FILE* f = std::fopen("address_latency.csv", "w")) {
        for (unsigned i = 0; i < lines; ++i) std::fprintf(f, "%u,%.1f\n", i, ns[i]);
        std::fclose(f);
        std::puts("saved address_latency.csv");
    }
}
