# Performance and cross-architecture validation

The current snapshot was collected on August 9, 2026 from one On-Demand
instance of each type, measuring the pair-lane bulk kernel (verification
value `0xA860AB01`). It is a reproducible comparison, not a portable
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
| haya32x64 pure JS | 64 | 22.9 | 22.0 | 34.0 | 34.1 |
| haya32x64 hybrid | 64 | 23.4 | 22.5 | 34.5 | 34.2 |
| hayahash64 BigInt | 64 | 506.7 | 529.9 | 666.9 | 642.8 |
| hayahash64 wasm | 64 | 49.4 | 49.3 | 81.0 | 80.0 |
| xxhash-wasm XXH64 | 64 | 39.8 | 40.1 | 64.4 | 72.3 |
| xxhashjs XXH64 | 64 | 1959.1 | 1977.5 | 2693.9 | 2687.5 |
| cyrb53 bytes | 53 | 3.8 | 5.8 | 6.1 | 8.6 |

| algorithm | bits | C8a | C8g |
|---|---|---|---|
| haya32x64 pure JS | 64 | 4620 | 3557 |
| haya32x64 hybrid | 64 | 9298 | 5655 |
| hayahash64 BigInt | 64 | 83 | 67 |
| hayahash64 wasm | 64 | 16088 | 11414 |
| xxhash-wasm XXH64 | 64 | 15928 | 10918 |
| xxhashjs XXH64 | 64 | 36 | 28 |
| cyrb53 bytes | 53 | 1109 | 786 |

The package crosses from pure JavaScript to wasm at 16 bytes. The pair-lane
kernel raised pure-JavaScript 1 MiB throughput 2.5x on C8a and 3.1x on C8g
over the previous kernel, and the wasm engine gained 35%/19% behind its
copy-bounded boundary. The boundary is visible in the public-API medians:

| engine | host | 15 B | 16 B | 17 B | 4 KiB |
|---|---|---|---|---|---|
| pure JS | C8a | 59.9 | 72.6 | 81.7 | 1031.2 |
| hybrid | C8a | 60.6 | 51.7 | 52.6 | 433.6 |
| pure JS | C8g | 109.1 | 184.2 | 195.4 | 1382.3 |
| hybrid | C8g | 109.8 | 81.5 | 82.9 | 783.1 |

### Streaming 1 MiB byte input

Each timed operation constructs a state, applies every pre-sliced chunk, and
materializes the final digest.

| algorithm | chunk | C8a | C8g |
|---|---|---|---|
| haya32x64 pure JS | 64 B | 690 | 363 |
| haya32x64 pure JS | 4096 B | 3921 | 3030 |
| haya32x64 pure JS | 65536 B | 4549 | 3840 |
| xxhash-wasm XXH64 | 64 B | 1130 | 616 |
| xxhash-wasm XXH64 | 4096 B | 14049 | 8560 |
| xxhash-wasm XXH64 | 65536 B | 16921 | 11113 |
| xxhashjs XXH64 | 64 B | 38 | 27 |
| xxhashjs XXH64 | 4096 B | 40 | 29 |
| xxhashjs XXH64 | 65536 B | 40 | 29 |

### ASCII string input

| algorithm | bits | C8a 8 chars | C8a 32 chars | C8g 8 chars | C8g 32 chars |
|---|---|---|---|---|---|
| haya32x64 pure JS | 64 | 269.6 | 364.7 | 487.0 | 656.9 |
| haya32x64 hybrid | 64 | 275.2 | 323.3 | 492.1 | 616.9 |
| hayahash64 BigInt | 64 | 842.5 | 1250.2 | 1244.2 | 1765.9 |
| hayahash64 wasm | 64 | 292.9 | 308.5 | 582.1 | 616.2 |
| xxhash-wasm XXH64 | 64 | 59.3 | 72.2 | 97.4 | 121.5 |
| xxhashjs XXH64 | 64 | 2452.5 | 3605.4 | 3326.9 | 4905.1 |
| cyrb53 string | 53 | 6.4 | 157.8 | 8.5 | 283.6 |
| imurmurhash | 32 | 22.9 | 55.1 | 43.6 | 102.4 |

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
| haya32x64 | 64 | 18.95 | 6.33 | 34.36 | 3.57 |
| khashv-64 | 64 | 46.16 | 2.78 | 105.52 | 1.60 |
| lookup3 | 64 | 12.13 | 1.49 | 35.22 | 0.86 |
| MurmurHash2-64.int32 | 64 | 16.40 | 3.31 | 32.14 | 2.67 |
| a5hash-32 | 32 | 7.41 | 4.51 | 21.46 | 2.95 |
| XXH-32 | 32 | 18.48 | 4.89 | 35.58 | 2.93 |
| t1ha0 | 64 | 18.72 | 5.21 | 35.61 | 3.63 |
| a5hash-128.64 | 64 | 7.36 | 15.53 | 20.48 | 10.07 |
| XXH3-64 | 64 | 13.97 | 61.02 | 25.69 | 10.76 |
| rapidhash | 64 | 13.49 | 19.50 | 25.78 | 8.94 |
| wyhash.strict | 64 | 13.20 | 11.03 | 28.44 | 6.80 |
| ChibiHash2 | 64 | 12.46 | 10.67 | 29.01 | 7.24 |
| MuseAir.bfast | 64 | 8.42 | 18.14 | 23.56 | 9.71 |

Within the close 32-bit-core set, haya32x64 has the highest bulk throughput
on both hosts: 91% above `MurmurHash2-64.int32` on C8a and 34% above it on
C8g, with 256 KiB bulk up 81%/31% over the previous kernel at essentially
unchanged 1-31-byte latency (18.95 versus 19.06 cycles/hash on C8a). Against
`khashv-64`, the passing-quality comparator in that set, haya32x64 is
2.4x/3.1x faster for 1-31-byte keys and 2.3x/2.2x faster in bulk on
C8a/C8g. Among the native-64-bit ceiling rows, `t1ha0` now measures 21%
behind haya32x64 in bulk on C8a and 2% ahead on C8g. MurmurHash2 and
lookup3 remain speed references rather than full-quality peers: the pinned
upstream raw reports pass 42/250 and 116/238 respectively. Short-key
latency still trails MurmurHash2 by 16% on C8a and 7% on C8g; seed
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
