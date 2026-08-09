# haya32x64

[![CI](https://img.shields.io/github/actions/workflow/status/thevilledev/haya32x64/ci.yml?branch=main&logo=githubactions&logoColor=white&label=CI)](https://github.com/thevilledev/haya32x64/actions/workflows/ci.yml)
[![license](https://img.shields.io/badge/license-Unlicense-blue.svg)](LICENSE)

A non-cryptographic 64-bit hash whose state and arithmetic are strictly
32-bit. The only wide primitive is the complete result of a 32×32 multiply;
there are no 64-bit state operations, 64-bit multiplies, SIMD instructions,
architecture-specific intrinsics, `BigInt`, or mandatory WebAssembly.

That constraint targets pure JavaScript, CSP-restricted and edge runtimes,
Cortex-M, RV32, and shader languages such as GLSL. On ordinary 64-bit hosts,
use a hash designed for that hardware instead—this project exists for the
next portability boundary down.

## Relationship to hayahash

Both projects produce 64-bit non-cryptographic digests, but they define
different hash functions:

- [`hayahash`](https://github.com/thevilledev/hayahash) uses native 64-bit
  state and ordinary 64×64-to-64-bit multiplication. Choose it when efficient
  `uint64_t`/`i64` arithmetic is available. Its JavaScript package is
  [`hayahash` on npm](https://www.npmjs.com/package/hayahash).
- `haya32x64` keeps its state and arithmetic 32-bit while retaining both halves
  of each 32×32 product. Choose it for 32-bit processors, pure JavaScript
  without `BigInt`, CSP-constrained runtimes, and similar targets.

They live in separate repositories because neither is an implementation
backend for the other: the same input and seed produce different digests.
Each algorithm therefore has its own reference header, known-answer vectors,
verification value, compatibility promise, versioning, and release lifecycle.
Switching between them is a persisted-data migration, not a performance toggle.

The algorithm definition is the single public-domain C header
[`haya32x64.h`](haya32x64.h). The [`js/`](js/) package contains two bit-exact
engines: a pure-JavaScript implementation based on `Math.imul` and exact
16-bit limbs, and the reference header compiled to a 1.6 KB wasm module for
bulk inputs. Both expose the digest and seed as `[low32, high32]`, so the API
itself does not need `BigInt`.

## Status and quality

The digest is experimental and currently frozen at verification value
`0xEAA8E435`. The streaming-capable candidate passed **188/188 SMHasher3
tests** at the repository's pinned upstream commit. That result is a self-run
on one host, not yet an upstream SMHasher3 result; see
[quality and reproduction](docs/quality.md) for the exact limits of the claim.

The production C header is bit-exact with that candidate. The repository also
checks:

- 180 shared known-answer vectors covering every dispatch boundary;
- the SMHasher3 verification-code construction in C and both one-shot JS engines;
- a deterministic C-reference differential corpus through hybrid, pure-JS, and
  streaming-JS modes;
- exhaustive and randomized one-shot/streaming split equivalence;
- input-bit and seed-bit avalanche smoke tests;
- exact collision batteries over structured keys; and
- every 64-bit seed with Hamming weight at most four (679,121 seeds), the
  regression that catches the earlier lossy seed-premix flaw.

This is still a non-cryptographic hash. Do not use it for authentication,
passwords, signatures, or attacker-controlled hash tables that need keyed
collision resistance.

## Usage

C, using split words throughout:

```c
#include "haya32x64.h"

haya32x64_words digest = haya32x64_hash(
    bytes, length,
    0xcafebabe, // seed low word
    0xdeadbeef  // seed high word
);
```

C callers already using `uint64_t` can use the convenience wrapper:

```c
uint64_t digest = haya32x64(bytes, length, UINT64_C(0xdeadbeefcafebabe));
```

Unknown-length C stream, with non-destructive `digest`:

```c
haya32x64_state state;
haya32x64_init(&state, 0xcafebabe, 0xdeadbeef);
haya32x64_update(&state, part1, part1_length);
haya32x64_update(&state, part2, part2_length);
haya32x64_words digest = haya32x64_digest(&state);
```

JavaScript:

```js
import { createHaya32x64, haya32x64, haya32x64Hex } from "haya32x64";

const [lo, hi] = haya32x64("hello world");
console.log(haya32x64Hex("hello world")); // a15ab6eb37d3a942

const stream = createHaya32x64();
stream.update(part1).update(part2);
const streamed = stream.digest();
```

The defined input-length domain is 0 through `UINT32_MAX` bytes. A null C
pointer is valid only when the length is zero.

## Performance snapshot

Measured August 9, 2026 on one EC2 C8a (AMD EPYC 9R45) and one C8g (Arm
Neoverse V2), using Node.js 24.18 / V8 13.6. JavaScript values are medians of
nine warmed public-API samples; MB/s is decimal.

| byte API | bits | C8a 4 B | C8g 4 B | C8a 1 MiB | C8g 1 MiB |
|---|---:|---:|---:|---:|---:|
| haya32x64 pure JS | 64 | **22.2 ns** | **34.1 ns** | **1,848 MB/s** | **1,160 MB/s** |
| haya32x64 hybrid | 64 | **23.3 ns** | **34.4 ns** | 6,884 MB/s | 4,765 MB/s |
| hayahash64 BigInt | 64 | 496.6 ns | 701.8 ns | 83 MB/s | 67 MB/s |
| hayahash64 wasm | 64 | 48.1 ns | 80.4 ns | 16,210 MB/s | 11,431 MB/s |
| xxhash-wasm XXH64 | 64 | 39.4 ns | 63.8 ns | 16,092 MB/s | 11,067 MB/s |
| cyrb53 bytes | 53 | 3.8 ns | 6.0 ns | 1,116 MB/s | 778 MB/s |

The pure-JavaScript engine now reuses one compact block kernel for one-shot
and streaming inputs. At 1 MiB that raised one-shot throughput by 74% on C8a
and 4.6x on C8g without changing the digest. Streaming with 64 KiB chunks
reached 1,819 MB/s and 1,113 MB/s respectively.

Native SMHasher3 speed tests, built with GCC 15.2 and `-O3 -march=native`,
measured `haya32x64` at 19.06 cycles/hash for 1–31-byte keys and 3.50
bytes/cycle in bulk on C8a; C8g measured 34.56 cycles/hash and 2.74
bytes/cycle. This is the highest bulk throughput in the report's close
64-bit-output, 32-bit-core comparison set on both hosts. Modern hashes using
native 64-bit arithmetic remain much faster, as expected; `haya32x64` is for
environments where that arithmetic is unavailable or expensive.

See [performance and cross-architecture validation](docs/benchmarks.md) for
expanded native, JavaScript, string, and streaming tables, methodology,
limitations, raw results, and reproduction commands.

## Development

```sh
make test

# C-reference differential replay through every JavaScript mode
cc -O2 -std=c11 -Wall -Wextra -Werror \
  tests/differential/generate.c -o tests/differential/generate
tests/differential/generate /tmp/haya32x64.bin 0x0123456789abcdef
HAYA32X64_CORPUS=/tmp/haya32x64.bin node --test js/test/differential.test.mjs
```

Documentation: [design](docs/design.md) · [quality](docs/quality.md) ·
[benchmarks](docs/benchmarks.md) · [SMHasher3](docs/smhasher3.md)

*Haya* (速) is Japanese for “fast.”
