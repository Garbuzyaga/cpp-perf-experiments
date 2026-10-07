// Core-to-core latency
//
// Two threads pinned to CPUs A and B pass a counter back and forth through
// one cache line. A writes an odd value and waits for B to answer with the
// next even one. Every hop moves the line from one core's cache to the
// other's, so round trip / 2 is the one-way latency between the two CPUs.
//
// Build:  g++ -std=c++20 -O2 -pthread core_latency.cpp -o core_latency
// Run:    ./core_latency [round_trips] [cpus]
// Output: matrix on stdout + core_latency.csv for plotting
 
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
 
// Spin loops are intentionally without _mm_pause(): on recent Intel cores
// pause takes ~140 cycles and would dominate the measurement.
double one_way_ns(unsigned cpu_a, unsigned cpu_b, std::uint64_t round_trips) {
    Line line;
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
 
double median_one_way_ns(unsigned a, unsigned b, std::uint64_t round_trips, int runs = 5) {
    std::vector<double> r;
    for (int i = 0; i < runs; ++i) r.push_back(one_way_ns(a, b, round_trips));
    std::sort(r.begin(), r.end());
    return r[r.size() / 2];
}
 
int main(int argc, char** argv) {
    unsigned hw = std::thread::hardware_concurrency();
    std::uint64_t round_trips = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 100'000ULL;
    unsigned cpus = argc > 2 ? std::atoi(argv[2]) : hw;
 
    if (cpus < 2) {
        std::puts("need at least 2 logical CPUs");
        return 1;
    }
 
    std::printf("cpus=%u  round_trips=%llu  (one-way latency, ns)\n\n",
                cpus, (unsigned long long)round_trips);
 
    std::vector<std::vector<double>> m(cpus, std::vector<double>(cpus, 0.0));
    for (unsigned a = 0; a < cpus; ++a) {
        for (unsigned b = a + 1; b < cpus; ++b) {
            m[a][b] = m[b][a] = median_one_way_ns(a, b, round_trips);
        }
    }
 
    std::printf("     ");
    for (unsigned b = 0; b < cpus; ++b) std::printf("%5u", b);
    std::puts("");
    for (unsigned a = 0; a < cpus; ++a) {
        std::printf("%4u ", a);
        for (unsigned b = 0; b < cpus; ++b) {
            if (a == b) std::printf("%5s", "-");
            else        std::printf("%5.0f", m[a][b]);
        }
        std::puts("");
    }
 
    if (FILE* f = std::fopen("core_latency.csv", "w")) {
        for (unsigned a = 0; a < cpus; ++a) {
            for (unsigned b = 0; b < cpus; ++b) {
                std::fprintf(f, b ? ",%.1f" : "%.1f", m[a][b]);
            }
            std::fputs("\n", f);
        }
        std::fclose(f);
        std::puts("\nsaved core_latency.csv");
    }
}