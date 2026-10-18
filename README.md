# cpp-perf-experiments

Small C++ benchmarks on things that matter for low-latency code, like
caches and atomics. Each folder has the code and the numbers from my machine.

| # | Topic |
|---|-------|
| 01 | [False sharing](01-false-sharing) |
| 02 | [Core to core latency](02-core-to-core-latency) |
| 03 | [Cost of memory order](03-memory-order) |

Your numbers will be different. If they're very different, let me know.
