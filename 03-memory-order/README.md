# Cost of memory_order

A loop that writes or reads one std::atomic with different memory orders.
One thread, no contention.

On x86 almost all of these compile to a plain `mov`. The exception is a
seq_cst store, which becomes `xchg` (gcc and clang both do this). `xchg`
with memory has an implicit lock prefix and waits until the store buffer
is drained. seq_cst is also the default, so `x = v` and `x.store(v)` pay
for it.

```cpp
x.store(v, std::memory_order_release);  // mov  QWORD PTR x[rip], rdi
x = v;                                  // xchg rdi, QWORD PTR x[rip]
```

## Results

i7-11700F (turbo on), Windows, g++ 13.1 -O2, 100M iterations, median of 7.

| variant               | ns/op |
|-----------------------|------:|
| store relaxed         | 0.16  |
| store release         | 0.22  |
| store seq_cst         | 4.14  |
| x = v                 | 4.24  |
| store release + fence | 4.34  |
| load relaxed          | 0.26  |
| load acquire          | 0.26  |
| load seq_cst          | 0.32  |

A seq_cst store is about 20x slower than a release store. Loads cost about
the same with any order.

4.14 ns is close to the cost of an uncontended `fetch_add` in
[01-false-sharing](../01-false-sharing) (4.2 ns). Both are locked
instructions.

The relaxed and release numbers are below one cycle per store. That's
throughput, not latency: the stores just go into the store buffer and the
core doesn't wait for them.

seq_cst isn't wrong, it's needed when several threads write to different
variables and everyone has to see those writes in the same order. For
"write data, then set a flag", release/acquire is enough.

## Build

```
g++ -std=c++20 -O2 memory_order.cpp -o memory_order
./memory_order
```

To see the asm: `g++ -std=c++20 -O2 -S memory_order.cpp` or paste the two
store functions into godbolt.org.

Post: [LinkedIn](https://lnkd.in/p/dgSevxQa)
