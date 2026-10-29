// Denormal (subnormal) floats
//
// The same loop over the same number of floats, only the values differ.
// Floats smaller than ~1.2e-38 are "subnormal". Many x86 CPUs handle
// arithmetic on them through a slow microcode path (a "microcode assist"),
// which can cost 100+ cycles per operation.
//
// Build:  g++ -std=c++20 -O2 denormals.cpp -o denormals
// Run:    ./denormals
 
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>
#include <xmmintrin.h>
#include <pmmintrin.h>
 
// y = y * a + b, repeated. With a < 1 and b = 0, y decays toward zero and,
// depending on the start value, stays in the normal or subnormal range.
// noinline so the compiler can't fold the loop away.
__attribute__((noinline)) float run(std::vector<float>& v, float a, float b, int rounds) {
    for (int r = 0; r < rounds; ++r) {
        for (float& x : v) x = x * a + b;
        // keep values from decaying to exactly zero: put them back
        for (float& x : v) x = x * (1.0f / a);
    }
    float sum = 0;
    for (float x : v) sum += x;
    return sum;
}
 
double ms(float start, int rounds) {
    std::vector<double> t;
    float sink = 0;
    for (int i = 0; i < 5; ++i) {
        std::vector<float> v(4096, start);
        auto s = std::chrono::steady_clock::now();
        sink += run(v, 0.5f, 0.0f, rounds);
        auto e = std::chrono::steady_clock::now();
        t.push_back(std::chrono::duration<double, std::milli>(e - s).count());
    }
    std::sort(t.begin(), t.end());
    std::printf("%s", sink == 12345.f ? " " : "");
    return t[2];
}
 
int main() {
    const int rounds = 20000;
    const float normal = 1.0f;
    const float subnormal = 1e-39f;  // below FLT_MIN (~1.18e-38)
 
    std::printf("4096 floats, %d rounds\n\n", rounds);
 
    double n = ms(normal, rounds);
    double d = ms(subnormal, rounds);
    std::printf("%-30s %8.1f ms\n", "normal values (1.0)", n);
    std::printf("%-30s %8.1f ms   %.0fx slower\n", "subnormal values (1e-39)", d, d / n);
 
    // Flush-to-zero + denormals-are-zero: treat subnormals as 0
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    double f = ms(subnormal, rounds);
    std::printf("%-30s %8.1f ms\n", "subnormal with FTZ/DAZ", f);
}