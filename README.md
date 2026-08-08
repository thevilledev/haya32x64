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

These are the candidate measurements that motivated the repository, not
portable guarantees. Pure JavaScript was measured on Node 22 / V8 12.4; C was
measured on one native host. They predate the streaming-capable digest and
have not yet been rerun. Re-measurement across engines and architectures is
part of the release bar.

| Node 22, pure JS | haya32x64 | cyrb53 (biased, 53-bit) | hayahash64 BigInt | hayahash64 wasm |
|---|---:|---:|---:|---:|
| bulk 1 MiB | **885 MB/s** | 731 | 102 | 16,448 |
| 4–8 byte keys | **47 ns** | 8–13 | 448–501 | 57–59 |

The reference C candidate measured 5.3 GB/s in bulk. The point is not to win
on 64-bit CPUs; it is to provide full 64-bit quality where efficient 64-bit
arithmetic is unavailable.

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
[SMHasher3](docs/smhasher3.md)

*Haya* (速) is Japanese for “fast.”
