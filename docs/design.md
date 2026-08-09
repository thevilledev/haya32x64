# Design

The reference header is the algorithm specification. This page gives the
short version of why the structure looks unusual for a 32-bit hash.

## Bulk data path

Inputs of at least 128 bytes use four pair-lanes over 32-byte blocks. Pair
`i` is the lane pair `(a, b) = (h[i], h[i+4])`, and each eight-byte stripe
enters one complete 32×32 multiply:

```text
u0 = w0 xor k0            u1 = w1 xor k1
m  = (u0 + a) * (u1 + b)
a  = low32(m) + rotl32(u1, 16)
b  = high32(m) xor u0
```

The per-pair key words `k0, k1` are the odd diffusion constants; each pair
uses a distinct overlapping constant pair. One multiply absorbs eight bytes,
which is what doubles bulk throughput over the previous one-multiply-per-word
kernel: multiplier ports were never the limit, total instruction count was.

Three details carry the quality argument:

- Both multiplier operands mix evolving, seed-derived state into the input.
  Steering an operand to zero requires knowing the lane state, and even a
  zeroed product still deposits both invertibly keyed words through the
  feedback, so no stripe is ever silently dropped. With all-zero input the
  operands become `k + state`, so the all-zero state is not a fixed point,
  and chaining the state through the product keeps block order significant,
  unlike purely additive accumulation.
- The half rotation on the raw feedback is load-bearing. With an unrotated
  `low32(m) + u1`, a difference of `2^b` in `w1` contributes
  `((u0 + a + 1) << b)` to the new `a`, which vanishes deterministically for
  half of all states at `b = 31`. SMHasher3's long-key Sparse, OneByte, and
  Long keysets found exactly that cancellation in an earlier candidate; the
  rotation separates the product difference from the raw difference at every
  bit position and costs nothing on the multiply's critical path.
- Pairs are otherwise independent, so the eight-to-four fold combines lane
  `a_i` with the neighbouring pair's `b_{(i+1) mod 4}`. A difference confined
  to one pair therefore reaches two folded words, each a bijection of its
  lane input, and those two words enter the two different finalizer products.
  An earlier candidate that folded each pair onto itself funneled
  pair-confined differences through one 32-bit word; SMHasher3 collided it at
  the expected 2^-32 rate, reproduced locally by a dedicated regression
  program before the rewire.

## Mid path, length paths, and tails

Below 128 bytes and for the bulk remainder, four lanes absorb 16-byte
stripes as

```text
t = word + rotl32(previous_word, 11)
product = (lane xor t) * KA
lane = low32(product)
carry = rotl32(carry, 5) xor high32(product)
```

Because `KA` is odd, the low-product lane update is a permutation for fixed
`t`. At the first position where two inputs differ, `previous_word` is still
equal, so `t` differs; the chained absorb sequence is injective by induction.
This is a statement about the absorb map, not a collision-resistance proof for
the complete hash. The serial carry definition can be reassociated without
changing any bit; the implementation uses a four-product grouped form on x86
and the literal serial form elsewhere.

The mid path sees fewer than the 32 stripes in the orbit of a 32-bit rotate
by 11. The 128-byte boundary is part of the digest definition, not a tuning
parameter. The bulk loop hands its final raw word to the mid path through
`previous`, so remainder stripes chain onto the bulk exactly as they chain
onto each other.

Tails use overlapping complete little-endian word loads. The two tail words
go through distinct bijective three-rotation maps:

```text
inja(w) = w xor rotl(w, 10) xor rotl(w, 21)
injb(w) = w xor rotl(w,  6) xor rotl(w, 25)
```

Inputs through eight bytes have a dedicated path with two independent complete
32×32 products.

## Seed and finalizer

The 64-bit seed is represented as two words. Two Feistel rounds make the seed
premix a bijection of the complete pair. The lane IVs depend only on that
premixed seed, so input can be absorbed before its final length is known. An
earlier design mixed each seed half through a lossy fold, creating a
birthday-scale 32-bit bottleneck. SMHasher3 `SeedSparse` found 13 collisions;
the local Hamming-weight battery preserves that regression.

Length is mixed in the finalizer as the complete product
`(len + KE) * KA`. Its low and high words enter different variable-by-variable
products before the fixed-odd cross-avalanche. A simpler spelling that xored
the length product into the two output words after those products failed
SMHasher3's `SeedZeroes` differential distribution, despite passing the local
quality ladder.

After the eight-to-four fold, the four lane words enter two complete 32×32
products as operands. Folding lanes to 32 bits before those products created
sequential collisions at roughly the 2^-32 scale in an earlier candidate. The
current cross-avalanche retains a 64-bit path through both output words.

## Streaming

`haya32x64_state` buffers totals below 192 bytes, where finalization can call
the tuned one-shot path directly. Once committed to bulk mode, `update`
consumes complete 32-byte blocks from stream offset zero and keeps 64–191 bytes
owned by the state. The two-block floor makes the final overlapping eight-byte
tail reads safe without retaining the entire input.

`haya32x64_digest` finalizes local copies of the lanes and counters. It neither
modifies the state nor prevents later updates, and its result is identical to
`haya32x64_hash` for every split of the same byte sequence.

## What “32-bit operations” means

State, rotates, xors, and additions are 32-bit. A multiply consumes two
32-bit operands and retains both result words. In C that result is expressed
with `uint64_t` because it is the portable spelling compilers lower to UMULL,
`MUL`+`MULHU`, or equivalent. The core never multiplies 64-bit operands or
feeds a 64-bit arithmetic result back as 64-bit state.

The `haya32x64_hash` API supplies seeds and digests as word pairs. The
streaming API uses the same representation. The `haya32x64` and
`haya32x64_digest64` convenience wrappers only split and join public
`uint64_t` values.
