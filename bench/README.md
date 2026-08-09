# Benchmarks

The benchmark suite separates native C, byte-oriented JavaScript, string
APIs, wasm boundary costs, and streaming. Do not combine these lanes into one
ranking: they exercise different portability constraints and output types.

## JavaScript

Install the pinned comparison packages and run the public-API benchmark:

```sh
npm ci --prefix bench
taskset -c 2 npm run bench --prefix bench > result.json
```

Omit `taskset` where it is unavailable. `make benchmark-js` performs the
install and run without OS-specific CPU pinning.

Defaults are nine 150 ms samples after per-case calibration and warmup. The
JSON result records every sample summary plus the OS, CPU, Node/V8 versions,
repository revision, and harness configuration. Override duration without
editing the harness:

```sh
HAYA_BENCH_SAMPLE_MS=300 HAYA_BENCH_SAMPLES=15 \
  npm run bench --prefix bench > result.json
```

For focused optimization runs, select algorithms by name substring:

```sh
HAYA_BENCH_FILTER=haya32x64 npm run bench --prefix bench > haya.json
```

All inputs are allocated before timing. The benchmark consumes every digest
through its public representation, so numbers include package dispatch,
output handling, UTF-8 conversion for string APIs, and JS/wasm copies.
Artifacts record both the base revision and whether the measured worktree was
dirty, preventing optimization runs from being mistaken for that base commit.

The comparison set contains the package's pure and hybrid engines,
`hayahash`'s BigInt and wasm engines, the native-wasm and pure-JavaScript
XXH64 packages, cyrb53, and imurmurhash. The last two have only 53 and 32
output bits respectively and appear as adoption/performance baselines, not as
equal-quality 64-bit alternatives. Package versions are exact in
`package-lock.json`.

After collecting JavaScript and native results from C8a and C8g machines,
render their comparable tables with:

```sh
node bench/report.mjs c8a-js.json c8g-js.json \
  c8a-native.txt c8g-native.txt
```

## Native C

Build the pinned SMHasher3 checkout with one set of optimization flags, then
run its small-key and bulk speed suites on one CPU:

```sh
make -C tests/smhasher3 \
  EXTRA_CXXFLAGS='-O3 -march=native -DNDEBUG -include cstdlib' build
HAYA_BENCH_CPU=2 ./bench/native.sh > native.txt
```

Set `HAYA_BENCH_FILTER=haya32x64` to run only matching entries while tuning.
An unset or empty filter runs the full comparator matrix.

SMHasher3 reports cycles per hash for every length from 1 through 31 bytes and
bytes per cycle for a 256 KiB bulk key. The script includes close 32-bit-core
comparisons, familiar 32-bit baselines, and modern 64-bit-native ceilings.
The forced standard-library include works around a missing upstream include
that GCC 15 diagnoses on AArch64; it does not change generated hash code.

The close 64-bit-output set is `haya32x64`, `khashv-64`, `lookup3`, and
`MurmurHash2-64.int32`. `a5hash-32` and `XXH-32` provide 32-bit-output floors;
`t1ha0`, `a5hash-128.64`, `XXH3-64`, rapidhash, wyhash, ChibiHash2, and
MuseAir provide native-64-bit ceilings. These are speed comparisons only:
inclusion does not claim that a hash passes SMHasher3's quality tests.

For stable cloud measurements, use an On-Demand compute-optimized instance,
leave the other CPUs idle, run on at least three fresh instances, and report
the distribution rather than a best-of value.

The checked-in August 2026 snapshot and raw artifacts are documented in
[`docs/benchmarks.md`](../docs/benchmarks.md).
