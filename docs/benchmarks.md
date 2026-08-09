# Performance and cross-architecture validation

The current snapshot was collected on August 9, 2026 from one On-Demand
instance of each type, measuring the pair-lane bulk kernel (verification
value `0x431563D2`). It is a reproducible comparison, not a portable
performance guarantee or a multi-instance distribution.

| host | processor | architecture | vCPUs |
|---|---|---|---:|
| EC2 `c8a.2xlarge` | AMD EPYC 9R45 | x86-64 | 8 |
| EC2 `c8g.2xlarge` | Arm Neoverse V2 | AArch64 | 8 |

Both hosts ran Ubuntu 26.04 with Linux 7.0.0-1010-aws, GCC 15.2.0, and
Node.js 24.18.0 / V8 13.6.233.17-node.50. Timed work was pinned to CPU 2
and the other CPUs were left idle.

## Validation performed

`make test` passed on both machines: the C known-answer, verification,
avalanche, collision, and streaming suites plus the JavaScript known-answer
and streaming suites. A separately generated 4,096-case C differential
corpus matched the hybrid, pure-JavaScript, and streaming-JavaScript engines
on both architectures.

The complete default SMHasher3 battery at the pinned commit passed on both
machines with the pair-lane kernel; see [quality](quality.md) for the exact
scope of that claim. The speed suites below measure speed only; inclusion of
a comparator is not a claim that it passes the statistical battery.

## Why the bulk kernel changed

Profiling showed the previous one-multiply-per-word kernel was never limited
by multiplier throughput: the measured hosts execute three (Zen 5) and two
(Neoverse V2) widening 32×32 multiplies per cycle. The limits were total
instruction count on x86 and serial dependency chains on AArch64. The
pair-lane kernel absorbs eight bytes per complete product, roughly halving
the work per byte; [design](design.md) documents the construction and the
two failed candidates that shaped its quality-relevant details. Inputs
below 128 bytes and the short-key paths are unchanged.

## JavaScript results

The harness runs nine 150 ms samples after calibration and warmup and
reports the median. Inputs are allocated before timing. Each operation uses
the package's public API and consumes the public digest representation, so
the figures include dispatch, output materialization, UTF-8 conversion for
string APIs, and JS/wasm copies. MB/s is decimal megabytes per second.

The hybrid `haya32x64` entry uses pure JavaScript below its wasm crossover
and the embedded reference wasm engine for larger inputs. `cyrb53` has only
53 usable output bits and `imurmurhash` has 32; their results are
market-relevant baselines, not equal-quality 64-bit substitutes.

### Byte input, one shot

| algorithm | bits | C8a 4 B | C8a 8 B | C8g 4 B | C8g 8 B |
|---|---|---|---|---|---|
| haya32x64 pure JS | 64 | 22.9 | 22.4 | 34.0 | 33.9 |
| haya32x64 hybrid | 64 | 23.2 | 22.4 | 34.2 | 34.3 |
| hayahash64 BigInt | 64 | 517.7 | 530.3 | 662.8 | 642.6 |
| hayahash64 wasm | 64 | 48.8 | 48.5 | 80.6 | 82.7 |
| xxhash-wasm XXH64 | 64 | 39.4 | 40.5 | 63.9 | 72.7 |
| xxhashjs XXH64 | 64 | 1939.3 | 1971.4 | 2691.0 | 2674.8 |
| cyrb53 bytes | 53 | 4.7 | 5.8 | 6.1 | 8.5 |

| algorithm | bits | C8a | C8g |
|---|---|---|---|
| haya32x64 pure JS | 64 | 4462 | 3886 |
| haya32x64 hybrid | 64 | 10971 | 6056 |
| hayahash64 BigInt | 64 | 82 | 67 |
| hayahash64 wasm | 64 | 16034 | 11488 |
| xxhash-wasm XXH64 | 64 | 15973 | 10738 |
| xxhashjs XXH64 | 64 | 37 | 28 |
| cyrb53 bytes | 53 | 1107 | 787 |

The package crosses from pure JavaScript to wasm at 16 bytes. The pair-lane
kernel raised pure-JavaScript 1 MiB throughput 2.8x on C8a and 3.3x on C8g
over the original kernel, and the wasm engine gained behind its
copy-bounded boundary. The boundary is visible in the public-API medians:

| engine | host | 15 B | 16 B | 17 B | 4 KiB |
|---|---|---|---|---|---|
| pure JS | C8a | 61.4 | 68.4 | 73.6 | 1055.5 |
| hybrid | C8a | 61.9 | 51.8 | 51.8 | 378.5 |
| pure JS | C8g | 109.8 | 184.4 | 195.4 | 1346.7 |
| hybrid | C8g | 110.3 | 82.5 | 84.7 | 742.3 |

### Streaming 1 MiB byte input

Each timed operation constructs a state, applies every pre-sliced chunk, and
materializes the final digest.

| algorithm | chunk | C8a | C8g |
|---|---|---|---|
| haya32x64 pure JS | 64 B | 666 | 380 |
| haya32x64 pure JS | 4096 B | 3906 | 2957 |
| haya32x64 pure JS | 65536 B | 4497 | 3722 |
| xxhash-wasm XXH64 | 64 B | 1130 | 616 |
| xxhash-wasm XXH64 | 4096 B | 14148 | 8550 |
| xxhash-wasm XXH64 | 65536 B | 16863 | 11126 |
| xxhashjs XXH64 | 64 B | 37 | 28 |
| xxhashjs XXH64 | 4096 B | 40 | 29 |
| xxhashjs XXH64 | 65536 B | 40 | 29 |

### ASCII string input

| algorithm | bits | C8a 8 chars | C8a 32 chars | C8g 8 chars | C8g 32 chars |
|---|---|---|---|---|---|
| haya32x64 pure JS | 64 | 267.9 | 362.1 | 489.6 | 660.3 |
| haya32x64 hybrid | 64 | 276.3 | 328.1 | 507.1 | 638.6 |
| hayahash64 BigInt | 64 | 822.9 | 1261.0 | 1230.8 | 1761.2 |
| hayahash64 wasm | 64 | 290.6 | 307.6 | 580.0 | 625.3 |
| xxhash-wasm XXH64 | 64 | 59.2 | 72.0 | 96.5 | 120.7 |
| xxhashjs XXH64 | 64 | 2437.8 | 3613.5 | 3332.5 | 4928.0 |
| cyrb53 string | 53 | 6.5 | 161.5 | 9.6 | 276.2 |
| imurmurhash | 32 | 22.8 | 57.4 | 43.7 | 103.4 |

## Native SMHasher3 results

SMHasher3 was built with `-O3 -march=native -DNDEBUG` and run with one
worker. Small-key results are its average cycles/hash over lengths 1 through
31 bytes. Bulk results are its average bytes/cycle over alignments 0 through
7 for a 256 KiB input. Cycles and bytes/cycle are preferable to
clock-derived GB/s when comparing the two processor families.

The first four entries are the closest 64-bit-output comparisons built
around 32-bit arithmetic. `a5hash-32` and `XXH-32` show the cost floor when
only a 32-bit digest is required. The remaining algorithms show what native
64-bit arithmetic and architecture-specific acceleration can achieve on
these 64-bit hosts.

| algorithm | bits | C8a cycles 1–31 B | C8a B/cycle | C8g cycles 1–31 B | C8g B/cycle |
|---|---|---|---|---|---|
| haya32x64 | 64 | 18.96 | 7.50 | 34.71 | 4.09 |
| khashv-64 | 64 | 46.19 | 2.78 | 105.70 | 1.60 |
| lookup3 | 64 | 12.14 | 1.49 | 35.20 | 0.86 |
| MurmurHash2-64.int32 | 64 | 16.43 | 3.31 | 32.01 | 2.67 |
| a5hash-32 | 32 | 7.35 | 4.53 | 21.47 | 2.95 |
| XXH-32 | 32 | 18.55 | 4.89 | 35.62 | 2.88 |
| t1ha0 | 64 | 18.74 | 5.21 | 35.52 | 3.64 |
| a5hash-128.64 | 64 | 7.36 | 15.53 | 20.48 | 10.07 |
| XXH3-64 | 64 | 13.99 | 60.59 | 25.72 | 10.76 |
| rapidhash | 64 | 13.51 | 19.47 | 25.78 | 8.94 |
| wyhash.strict | 64 | 13.21 | 11.02 | 28.43 | 6.79 |
| ChibiHash2 | 64 | 12.45 | 10.48 | 29.14 | 7.25 |
| MuseAir.bfast | 64 | 8.42 | 18.14 | 23.59 | 9.77 |

Within the close 32-bit-core set, haya32x64 has the highest bulk throughput
on both hosts: 2.3x `MurmurHash2-64.int32` on C8a and 1.5x on C8g, with
256 KiB bulk at 2.1x/1.5x the original kernel at essentially unchanged
1-31-byte latency (18.96 versus 19.06 cycles/hash on C8a). Against
`khashv-64`, the passing-quality comparator in that set, haya32x64 is
2.4x/3.0x faster for 1-31-byte keys and 2.7x/2.6x faster in bulk on
C8a/C8g. It also now exceeds every 32-bit-arithmetic row in the wider
table on both hosts, including `t1ha0` (by 44% on C8a and 12% on C8g) and
the 32-bit-output floors `a5hash-32` and `XXH-32`. MurmurHash2 and lookup3
remain speed references rather than full-quality peers: the pinned
upstream raw reports pass 42/250 and 116/238 respectively. Short-key
latency still trails MurmurHash2 by 15% on C8a and 8% on C8g; seed
premixing and the wider finalizer account for that quality cost.

## Reproduction and raw data

The harness, pinned npm dependencies, comparator rationale, and commands are
in [`bench/`](../bench/README.md). Raw artifacts for this snapshot:

- [C8a Node.js results](../bench/results/c8a-node24-v2.json)
- [C8g Node.js results](../bench/results/c8g-node24-v2.json)
- [C8a SMHasher3 output](../bench/results/c8a-smhasher3-v2.txt)
- [C8g SMHasher3 output](../bench/results/c8g-smhasher3-v2.txt)

The previous kernel's artifacts remain under the same directory for
comparison. This snapshot has one instance of each type, one Node/V8
version, and one compiler. Repeat it on at least three fresh instances
before treating small differences as durable.
