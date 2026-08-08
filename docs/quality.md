# Quality and verification

## SMHasher3 result

The streaming-capable candidate passed 188/188 default SMHasher3 tests with
canonical verification value `0xEAA8E435` (`0x8705401D` for the byte-swapped
variant).
The run used the pinned commit documented in [SMHasher3 setup](smhasher3.md).

Important scope: this is currently a self-run result on one host and the
adapter in `tests/smhasher3/haya32x64.cpp`. The shipped header has been checked
bit-for-bit against that adapter across 100,000 randomized inputs and the
shared vectors, but an upstream SMHasher3 row and a multi-host/compiler matrix
remain release work. “Passes SMHasher3” is evidence against known statistical
and structural flaws, not evidence of cryptographic security.

## Tests in this repository

`make -C tests test` runs four independent layers:

1. `kat.c` regenerates 180 C digests and diffs them against `kat.txt`. Lengths
   cover the short path, every 16/32-byte transition, the 128-byte bulk entry,
   bulk remainders, and larger inputs under three full-width seeds.
2. `verify.c` reconstructs SMHasher3's verification-code input and pins the
   canonical low word to `0xEAA8E435`.
3. `quality.c` measures input/seed strict-avalanche smoke tests and checks exact
   collisions over sequential inputs, high-byte-only stripes, zero extension,
   top-bit combinations, and all seeds of Hamming weight zero through four.
4. `streaming.c` checks every two-way split through 2 KiB, fixed adversarial
   chunk patterns, randomized lengths through 64 KiB, non-destructive digest,
   and continued updates after digest.

The sparse-seed set has 679,121 members. A good 64-bit output has only about a
1.25×10^-8 chance of an accidental collision at that size, while the rejected
32-bit-width premix fails near birthday scale.

The JavaScript suite consumes the same committed KAT and independently rebuilds
the SMHasher3 verification value through both engines. The differential
generator emits randomized inputs and C-reference digests; the pure-JS,
streaming-JS, and wasm engines must consume the identical corpus without a
mismatch.

## Portability coverage

CI builds with GCC and Clang, runs ASan+UBSan, rebuilds the wasm artifact, and
replays the C differential corpus in JavaScript. The workflow also runs KAT
and streaming equivalence on a big-endian s390x target and under MSVC, so
alignment, byte order, state layout, and compiler ABI assumptions are tested
rather than inferred.

Before a stable release, the quality claim should be repeated on multiple
physical hosts, compiler versions, 32-bit architectures, and JavaScript
engines. Performance figures likewise need fresh distributions rather than a
single best run.
