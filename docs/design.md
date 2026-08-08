# Design

The reference header is the algorithm specification. This page gives the
short version of why the structure looks unusual for a 32-bit hash.

## Data path

Inputs of at least 128 bytes use eight `uint32_t` lanes over 32-byte blocks.
Each stripe is absorbed as

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
the complete hash.

Ordinary 32-bit hashes discard the high half of each product, even though that
is the half that diffuses high input bits downward. haya32x64 folds those high
words into a serial accumulator kept off the lane-critical path. A raw final
stripe is also added to lane zero once per 32-byte block; that checkpoint
breaks difference ladders that follow the rotation orbit around the lanes.

## Length paths and tails

The four-lane mid path is used only below 128 bytes and for the final bulk
remainder. It therefore sees fewer than the 32 stripes in the orbit of a
32-bit rotate by 11. The boundary is part of the digest definition, not a
tuning parameter.

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
