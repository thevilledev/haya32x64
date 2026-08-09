# Performance and cross-architecture validation

The current snapshot was collected on August 9, 2026 from one On-Demand
instance of each type. It is a reproducible comparison, not a portable
performance guarantee or a multi-instance distribution.

| host | processor | architecture | vCPUs |
|---|---|---|---:|
| EC2 `c8a.2xlarge` | AMD EPYC 9R45 | x86-64 | 8 |
| EC2 `c8g.2xlarge` | Arm Neoverse V2 | AArch64 | 8 |

Both hosts ran Ubuntu 26.04 with Linux 7.0.0-1010-aws, GCC 15.2.0, and
Node.js 24.18.0 / V8 13.6.233.17-node.50. The optimized worktree was based on
revision `a98c011de2e36ba272bdbbf8c7891f64a734295a`; the focused artifacts mark
it as dirty so they cannot be mistaken for that base commit. The digest and
verification values are unchanged. Timed work was pinned to CPU 2 and the
other CPUs were left idle.

## Validation performed

`make test` passed on both machines. That covers the C known-answer,
verification, avalanche, collision, and streaming suites plus the JavaScript
known-answer and streaming suites. The opt-in differential test was then run
with a separately generated 4,096-case C corpus; the hybrid, pure-JavaScript,
and streaming-JavaScript engines matched every C digest on both architectures.

SMHasher3's `SpeedSmall` and `SpeedBulk` suites also completed for every native
comparator below. These runs measure speed only. The existing 188/188 quality
result and its narrower scope are documented in [quality and
verification](quality.md).

## What limited the original implementation

The algorithm was not waiting on another statistical compromise. Three code
generation issues accounted for the gap:

- The source updated the high-product carry after every lane. Although lane
  products are independent, that spelling presented compilers with a serial
  rotate/xor chain around them.
- One-shot JavaScript duplicated the full eight-lane loop. Its large function
  compiled reasonably on x86 V8 but disastrously on AArch64 V8; the already
  compact streaming loop exposed that discrepancy.
- One generic native loop schedule could not cover both AMD and Arm multiply
  latency. The best measured x86 kernel groups four products and unrolls two
  blocks. AArch64 groups products in pairs and unrolls eight blocks. The large
  kernels are kept out of line so short-key code does not inherit their code
  size and register pressure.

All transformations are algebraic scheduling changes. The C KATs, both
SMHasher3 endian verification values, streaming boundary suite, and 4,096-case
C-to-JavaScript differential corpus remained bit-exact on both hosts.

## JavaScript results

The harness runs nine 150 ms samples after calibration and warmup and reports
the median. Inputs are allocated before timing. Each operation uses the
package's public API and consumes the public digest representation, so the
figures include dispatch, output materialization, UTF-8 conversion for string
APIs, and JS/wasm copies. MB/s is decimal megabytes per second.

The hybrid `haya32x64` entry uses pure JavaScript below its wasm crossover and
the embedded reference wasm engine for larger inputs. `cyrb53` has only 53
usable output bits and `imurmurhash` has 32; their results are market-relevant
baselines, not equal-quality 64-bit substitutes.

### Byte input, one shot

| algorithm | bits | C8a 4 B ns | C8a 8 B ns | C8g 4 B ns | C8g 8 B ns |
|---|---:|---:|---:|---:|---:|
| haya32x64 pure JS | 64 | 22.2 | 22.1 | 34.1 | 34.0 |
| haya32x64 hybrid | 64 | 23.3 | 23.3 | 34.4 | 34.5 |
| hayahash64 BigInt | 64 | 496.6 | 527.5 | 701.8 | 638.6 |
| hayahash64 wasm | 64 | 48.1 | 48.8 | 80.4 | 79.9 |
| xxhash-wasm XXH64 | 64 | 39.4 | 40.1 | 63.8 | 72.1 |
| xxhashjs XXH64 | 64 | 1,927.6 | 1,929.5 | 2,660.6 | 2,669.6 |
| cyrb53 bytes | 53 | 3.8 | 5.8 | 6.0 | 8.5 |

| algorithm | bits | C8a 1 MiB MB/s | C8g 1 MiB MB/s |
|---|---:|---:|---:|
| haya32x64 pure JS | 64 | 1,848 | 1,160 |
| haya32x64 hybrid | 64 | 6,884 | 4,765 |
| hayahash64 BigInt | 64 | 83 | 67 |
| hayahash64 wasm | 64 | 16,210 | 11,431 |
| xxhash-wasm XXH64 | 64 | 16,092 | 11,067 |
| xxhashjs XXH64 | 64 | 39 | 28 |
| cyrb53 bytes | 53 | 1,116 | 778 |

The package crosses from pure JavaScript to wasm at 16 bytes. The boundary is
visible in the public-API medians:

| engine | host | 15 B ns | 16 B ns | 17 B ns | 4 KiB ns |
|---|---|---:|---:|---:|---:|
| pure JS | C8a | 61.5 | 71.5 | 79.5 | 2,449.5 |
| hybrid | C8a | 62.5 | 49.8 | 50.4 | 592.4 |
| pure JS | C8g | 108.5 | 127.9 | 140.1 | 3,768.0 |
| hybrid | C8g | 108.7 | 79.3 | 80.8 | 899.6 |

Reusing the compact streaming block loop for one-shot inputs removed the V8
backend cliff. Pure-JavaScript 1 MiB throughput rose from 1,062 to 1,848 MB/s
on C8a and from 251 to 1,160 MB/s on C8g. A separate compact 9–15-byte path
also prevents bulk-kernel changes from perturbing V8's shortest nontrivial
path.

### Streaming 1 MiB byte input

Each timed operation constructs a state, applies every pre-sliced chunk, and
materializes the final digest.

| algorithm | chunk | C8a MB/s | C8g MB/s |
|---|---:|---:|---:|
| haya32x64 pure JS | 64 B | 551 | 295 |
| haya32x64 pure JS | 4 KiB | 1,666 | 1,027 |
| haya32x64 pure JS | 64 KiB | 1,819 | 1,113 |
| xxhash-wasm XXH64 | 64 B | 1,115 | 631 |
| xxhash-wasm XXH64 | 4 KiB | 13,881 | 8,579 |
| xxhash-wasm XXH64 | 64 KiB | 16,726 | 11,137 |
| xxhashjs XXH64 | 64 B | 38 | 28 |
| xxhashjs XXH64 | 4 KiB | 40 | 29 |
| xxhashjs XXH64 | 64 KiB | 40 | 29 |

### ASCII string input

| algorithm | bits | C8a 8 chars ns | C8a 32 chars ns | C8g 8 chars ns | C8g 32 chars ns |
|---|---:|---:|---:|---:|---:|
| haya32x64 pure JS | 64 | 269.1 | 362.3 | 486.0 | 653.2 |
| haya32x64 hybrid | 64 | 271.6 | 325.0 | 506.2 | 618.9 |
| hayahash64 BigInt | 64 | 830.9 | 1,238.0 | 1,229.5 | 1,788.8 |
| hayahash64 wasm | 64 | 290.3 | 306.7 | 585.0 | 618.9 |
| xxhash-wasm XXH64 | 64 | 59.5 | 71.8 | 96.6 | 119.8 |
| xxhashjs XXH64 | 64 | 2,415.1 | 3,598.4 | 3,312.5 | 4,846.4 |
| cyrb53 string | 53 | 6.4 | 156.7 | 9.6 | 287.6 |
| imurmurhash | 32 | 23.6 | 55.9 | 43.3 | 101.6 |

## Native SMHasher3 results

SMHasher3 was built with `-O3 -march=native -DNDEBUG` and run with one worker.
Small-key results are its average cycles/hash over lengths 1 through 31 bytes.
Bulk results are its average bytes/cycle over alignments 0 through 7 for a
256 KiB input. Cycles and bytes/cycle are preferable to clock-derived GB/s
when comparing the two processor families.

The first four entries are the closest 64-bit-output comparisons built around
32-bit arithmetic. `a5hash-32` and `XXH-32` show the cost floor when only a
32-bit digest is required. The remaining algorithms show what native 64-bit
arithmetic and architecture-specific acceleration can achieve on these
64-bit hosts.

| algorithm | bits | C8a cycles 1–31 B | C8a B/cycle | C8g cycles 1–31 B | C8g B/cycle |
|---|---:|---:|---:|---:|---:|
| haya32x64 | 64 | 19.06 | 3.50 | 34.56 | 2.74 |
| khashv-64 | 64 | 46.10 | 2.78 | 105.47 | 1.60 |
| lookup3 | 64 | 12.12 | 1.49 | 35.23 | 0.86 |
| MurmurHash2-64.int32 | 64 | 16.39 | 3.32 | 32.14 | 2.67 |
| a5hash-32 | 32 | 7.34 | 4.52 | 21.47 | 2.95 |
| XXH-32 | 32 | 18.47 | 4.90 | 35.57 | 2.93 |
| t1ha0 | 64 | 18.70 | 5.22 | 35.55 | 3.65 |
| a5hash-128.64 | 64 | 7.33 | 15.54 | 20.46 | 10.08 |
| XXH3-64 | 64 | 13.95 | 60.59 | 25.70 | 10.76 |
| rapidhash | 64 | 13.49 | 19.51 | 25.77 | 8.95 |
| wyhash.strict | 64 | 13.18 | 11.03 | 28.43 | 6.79 |
| ChibiHash2 | 64 | 12.45 | 10.70 | 29.14 | 7.27 |
| MuseAir.bfast | 64 | 8.41 | 18.13 | 23.57 | 9.73 |

SMHasher3 provides an appropriate common native timing harness, but it cannot
measure JavaScript package behavior, wasm boundary costs, or string APIs. The
separate JavaScript harness exists for those lanes. A speed row is also not a
quality endorsement: digest widths differ, and not every comparator passes
SMHasher3's statistical suite.

Within the close 32-bit-core set, optimized `haya32x64` has the highest bulk
throughput on both hosts: 5.4% above `MurmurHash2-64.int32` on C8a and 2.6%
above it on C8g. MurmurHash2 and lookup3 are useful speed references, not
full-quality peers: the pinned upstream raw reports pass 42/250 and 116/238
respectively, versus 250/250 for `khashv-64`. Against that passing but much
slower comparator, haya32x64 is 2.4x/3.1x faster for 1–31-byte keys and
1.26x/1.71x faster in bulk on C8a/C8g. Short-key latency still trails
MurmurHash2 by 16% on C8a and 8% on C8g; seed premixing and the wider
finalizer account for that quality cost.

## Reproduction and raw data

The harness, pinned npm dependencies, comparator rationale, and commands are
in [`bench/`](../bench/README.md). The original full-matrix artifacts retain
all comparator rows; focused optimized artifacts contain every haya32x64 case
and record `dirty: true` plus the base revision:

- [C8a Node.js results](../bench/results/c8a-node24.json)
- [C8g Node.js results](../bench/results/c8g-node24.json)
- [C8a SMHasher3 output](../bench/results/c8a-smhasher3.txt)
- [C8g SMHasher3 output](../bench/results/c8g-smhasher3.txt)
- [C8a optimized Node.js results](../bench/results/c8a-node24-optimized.json)
- [C8g optimized Node.js results](../bench/results/c8g-node24-optimized.json)
- [C8a optimized SMHasher3 output](../bench/results/c8a-smhasher3-optimized.txt)
- [C8g optimized SMHasher3 output](../bench/results/c8g-smhasher3-optimized.txt)

Across the focused optimized JavaScript cases, the largest p10-to-p90 time
spread was 3.3% on C8a and 8.2% on C8g. This snapshot still has only one
instance of each type, one Node/V8 version, and one compiler. Repeat it on at
least three fresh instances before treating small differences as durable.
