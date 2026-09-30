# False sharing

Each thread bumps its own counter. When the counters sit in one 64-byte
cache line, the cores keep taking the line from each other, even though
no data is actually shared.

I expected the packed version to be slow and `alignas(64)` to fix it.
With plain load/store increments the difference turned out to be close to
zero. My understanding is that the store buffer hides it: the core reads
its last write from the buffer and writes to cache in the background, so
the loop doesn't wait for the line. `fetch_add` needs the line in exclusive
state on every increment, and there the effect is large.

## Results

i7-11700F (8C/16T, turbo on), Windows, g++ 13 -O2, one thread per physical
core, median of 7 runs, 100M increments per thread. ns/op is per thread.

| threads | packed store | padded store | local | packed fetch_add | padded fetch_add |
|--------:|-------------:|-------------:|------:|-----------------:|-----------------:|
| 2       | 1.60         | 1.53         | 0.27  | 19.3             | 4.2              |
| 4       | 1.71         | 1.60         | 0.27  | 33.7             | 4.3              |
| 8       | 1.97         | 1.72         | 0.29  | 59.5             | 5.0              |

With packed atomics, every thread count was about 2x slower than a single
thread doing all the increments alone. Numbers vary by about 10% between runs.

## Build

```
g++ -std=c++20 -O2 -pthread false_sharing.cpp -o false_sharing
./false_sharing 4
```

Threads are pinned to logical CPUs 0, 2, 4, ... so that each one lands on
its own physical core. If your CPU numbers SMT siblings differently
(check `lscpu -e`), change `kCpuStride`.

Post: [LinkedIn](https://lnkd.in/p/ehX5hRJ4)
