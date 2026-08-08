# Quality and verification

## SMHasher3 result

The final candidate passed 188/188 default SMHasher3 tests with canonical
verification value `0x7137E6DC` (`0x1DFB2C55` for the byte-swapped variant).
The run used the pinned commit documented in [SMHasher3 setup](smhasher3.md).

Important scope: this is currently a self-run result on one host and the
candidate adapter in `exp/hayaexp.cpp`. The shipped header has been checked
bit-for-bit against that candidate across 100,000 randomized inputs and the
shared vectors, but an upstream SMHasher3 row and a multi-host/compiler matrix
remain release work. “Passes SMHasher3” is evidence against known statistical
and structural flaws, not evidence of cryptographic security.

## Tests in this repository

`make -C tests test` runs three independent layers:

1. `kat.c` regenerates 180 C digests and diffs them against `kat.txt`. Lengths
   cover the short path, every 16/32-byte transition, the 128-byte bulk entry,
   bulk remainders, and larger inputs under three full-width seeds.
2. `verify.c` reconstructs SMHasher3's verification-code input and pins the
   canonical low word to `0x7137E6DC`.
3. `quality.c` measures input/seed strict-avalanche smoke tests and checks exact
   collisions over sequential inputs, high-byte-only stripes, zero extension,
   top-bit combinations, and all seeds of Hamming weight zero through four.

The sparse-seed set has 679,121 members. A good 64-bit output has only about a
1.25×10^-8 chance of an accidental collision at that size, while the rejected
32-bit-width premix fails near birthday scale.

The JavaScript suite consumes the same committed KAT and independently rebuilds
the SMHasher3 verification value through both engines. The differential
generator emits randomized inputs and C-reference digests; both the pure-JS
and wasm engines must consume the identical corpus without a mismatch.

## Portability coverage

CI builds with GCC and Clang, runs ASan+UBSan, rebuilds the wasm artifact, and
replays the C differential corpus in JavaScript. The workflow also includes a
big-endian s390x KAT and an MSVC KAT so alignment, byte order, and compiler ABI
assumptions are tested rather than inferred.

Before a stable release, the quality claim should be repeated on multiple
physical hosts, compiler versions, 32-bit architectures, and JavaScript
engines. Performance figures likewise need fresh distributions rather than a
single best run.
