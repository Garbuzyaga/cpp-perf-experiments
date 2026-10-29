# Denormal floats

The same loop over 4096 floats, with the same operations. The only
difference is the starting value: 1.0 or 1e-39.

Floats below about 1.2e-38 are subnormal (denormal). They exist so that
values can get closer to zero gradually, but on many x86 CPUs math with
them doesn't go through the normal fast path. The CPU falls back to a
microcode assist, which costs much more per operation.

## Results

i7-11700F (turbo on), Windows, g++ 13.1 -O2, 20000 rounds, median of 5.

| values                  | time      |
|-------------------------|----------:|
| normal (1.0)            | 43.7 ms   |
| subnormal (1e-39)       | 4118.9 ms |
| subnormal with FTZ/DAZ  | 45.4 ms   |

94x slower, and nothing in the code tells you why. A profiler shows the
hot loop, but not that the problem is the values.

## Fix

FTZ (flush to zero) and DAZ (denormals are zero) make the CPU treat
subnormals as 0:

```cpp
#include <xmmintrin.h>
#include <pmmintrin.h>

_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
```

These are bits in the MXCSR register, so they apply per thread. Set them
in every thread that does the math.

`-ffast-math` in gcc also turns them on at program start, but it changes
other floating point rules too, so I wouldn't use it just for this.

You lose some precision near zero. For most code (signal processing,
smoothing, things that decay to zero) that doesn't matter.

## Build

```
g++ -std=c++20 -O2 denormals.cpp -o denormals
./denormals
```

Post: [LinkedIn](https://lnkd.in/p/d_bVpcGW)
