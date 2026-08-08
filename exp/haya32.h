// haya32x64 - experimental: a 64-bit hash whose every operation is
// 32-bit. The only multiply is 32x32->64 (one UMULL on Cortex-M3+,
// MUL+MULHU on RV32, umulExtended in GLSL, ~4 Math.imul in JS), and
// all state words, rotates, adds and xors are 32-bit. The aim is
// hayahash-grade quality one portability tier below hayahash64:
// targets with no efficient 64-bit multiplier at all.
//
// Structure notes (each mirrors a hayahash64 lesson, halved):
//  - 8 u32 lanes over 32-byte blocks; absorb t = w + rotl32(wp, 11):
//    the same first-difference induction makes the absorb sequence
//    injective; the rotated copy plants every stripe bit at a low
//    position in the next lane where + and rotl commute with neither
//    GF(2) nor mod-2^32 algebra.
//  - Lane update h = lo32((h ^ t) * KA) is a bijection of h (KA odd),
//    exactly hayahash's lane recurrence mod 2^32. The product's HIGH
//    half - the downward diffusion that 32-bit-only designs like
//    xxh32/Murmur3-x86 never harvest - is folded into a serial
//    carry accumulator C = rotl32(C, 5) ^ hi32(product), off the
//    lane-critical path, and injected at the 8->4 lane fold.
//  - Per-block checkpoint h0 += (raw last stripe) blocks
//    rotation-orbit ladders, as in hayahash64. The 4-lane mid path is
//    only used below 128 bytes = 31 stripes, under the 32-stripe
//    orbit of rotl-11 (mirror of the 320-byte / 64-stripe rule).
//  - Tails read overlapping whole u32 words from the end; a dedicated
//    <= 8-byte path spreads both words with distinct 3-rotation
//    bijections and two independent 32x64 products.
//  - Seed is a full uint64_t, handled as two u32 words premixed with
//    the length into (s0, s1); every lane IV derives from those.
//  - Finalizer: two 32x32->64 "mum" products cross the folded state
//    halves so both digest words depend on everything, then a
//    xorshift-multiply avalanche per half with cross injection.
//
// Public domain (Unlicense), same as hayahash.

#ifndef HAYA32_H
#define HAYA32_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define HAYA32_BE 1
#else
#define HAYA32_BE 0
#endif

static inline uint32_t haya32_load32(const uint8_t *p)
{
	uint32_t v;
	memcpy(&v, p, 4);
#if HAYA32_BE
	v = (v >> 24) | ((v >> 8) & 0xFF00u) | ((v & 0xFF00u) << 8) | (v << 24);
#endif
	return v;
}
static inline uint32_t haya32_rotl(uint32_t x, int n)
{
	return (x << n) | (x >> (-n & 31));
}

// xxHash32 / Murmur3 constants: odd, battle-tested diffusion.
#define HAYA32_KA 0x9E3779B1u
#define HAYA32_KB 0x85EBCA77u
#define HAYA32_KC 0xC2B2AE3Du
#define HAYA32_KD 0x27D4EB2Fu
#define HAYA32_KE 0x165667B1u
#define HAYA32_F1 0x85EBCA6Bu
#define HAYA32_F2 0xC2B2AE35u

// Bijective 3-rotation spreads; distinct amounts per word so the two
// short-path multiply terms can never be erased simultaneously.
static inline uint32_t haya32_inja(uint32_t w)
{
	return w ^ haya32_rotl(w, 10) ^ haya32_rotl(w, 21);
}
static inline uint32_t haya32_injb(uint32_t w)
{
	return w ^ haya32_rotl(w, 6) ^ haya32_rotl(w, 25);
}

// One absorb: bijective in h; hi half goes to the caller's C.
#define HAYA32_LANE(h, t, C) do { \
	uint64_t p64_ = (uint64_t)((h) ^ (t)) * HAYA32_KA; \
	(h) = (uint32_t)p64_; \
	(C) = haya32_rotl((C), 5) ^ (uint32_t)(p64_ >> 32); \
} while (0)

// Final avalanche. The four lanes enter the two wide products as
// OPERANDS - never pre-folded to 32 bits - so no key class that
// varies only a lane pair meets a bottleneck narrower than the
// digest (the absorb chain guarantees at least two adjacent lanes
// vary). Each product covers one lane of the pair; the raw rotated
// lane injections keep a lane alive even if the other product
// operand lands on zero for some (seed, length) slice. C carries the
// xor-accumulated product high halves.
static inline uint64_t
haya32_final(uint32_t h0, uint32_t h1, uint32_t h2, uint32_t h3,
             uint32_t s0, uint32_t s1, uint32_t C)
{
	h1 ^= C;
	h3 ^= haya32_rotl(C, 16);
	uint64_t m0 = (uint64_t)(h0 ^ s0) * (h2 ^ HAYA32_KC);
	uint64_t m1 = (uint64_t)(h1 ^ HAYA32_KD) *
	              (h3 ^ haya32_rotl(s1, 11));
	uint32_t x = (uint32_t)m0 ^ (uint32_t)(m1 >> 32) ^
	             haya32_rotl(h1, 7);
	uint32_t y = (uint32_t)(m0 >> 32) ^ (uint32_t)m1 ^
	             haya32_rotl(h0, 19);
	// Cross-avalanche the pair (each step bijective on (x, y)).
	x ^= x >> 16; x *= HAYA32_F1;
	y ^= y >> 15; y *= HAYA32_F2;
	x ^= haya32_rotl(y, 13);
	y ^= x >> 16;
	x *= HAYA32_KB;
	x ^= x >> 15;
	y *= HAYA32_KE;
	y ^= y >> 14;
	return ((uint64_t)y << 32) | x;
}

// len must fit in 32 bits for the prototype.
static inline uint64_t
haya32x64(const void *keyIn, ptrdiff_t len, uint64_t seed)
{
	const uint8_t *p = (const uint8_t *)keyIn;
	ptrdiff_t l = len;
	const uint32_t slo = (uint32_t)seed, shi = (uint32_t)(seed >> 32);
	const uint32_t lw = (uint32_t)len;

	// Premix: seed words and length -> (s0, s1); all IVs derive here.
	// Two Feistel rounds keep (s0, s1) a BIJECTION of the 64-bit seed
	// (per-half lossy folds collided sparse seeds at 32-bit birthday
	// scale - found by SMHasher3 SeedSparse); the len product is
	// injective in len, so cross-length safety holds too.
	uint64_t q1 = (uint64_t)(shi ^ HAYA32_KC) * HAYA32_KD;
	uint32_t u  = slo ^ (uint32_t)q1 ^ (uint32_t)(q1 >> 32);
	uint64_t q0 = (uint64_t)(u ^ HAYA32_KA) * HAYA32_KB;
	uint32_t v  = shi ^ (uint32_t)q0 ^ (uint32_t)(q0 >> 32);
	uint64_t q2 = (uint64_t)(lw + HAYA32_KE) * HAYA32_KA;
	uint32_t s0 = u ^ (uint32_t)q2;
	uint32_t s1 = v ^ (uint32_t)(q2 >> 32);

	if (l <= 8) {
		uint32_t a, b;
		if (l >= 4) {
			a = haya32_load32(p);
			b = haya32_load32(p + l - 4);
		} else if (l > 0) {
			a = p[0];
			b = ((uint32_t)p[l >> 1] << 8) | ((uint32_t)p[l - 1] << 16);
		} else {
			a = 0; b = 0;
		}
		uint64_t x64 = (uint64_t)(haya32_inja(a) ^ s0 ^ HAYA32_KB) *
		               HAYA32_KA;
		uint64_t y64 = (uint64_t)(haya32_injb(b) ^
		               haya32_rotl(s1, 13) ^ HAYA32_KE) * HAYA32_KC;
		return haya32_final((uint32_t)x64, (uint32_t)(x64 >> 32),
		                    (uint32_t)y64, (uint32_t)(y64 >> 32),
		                    s0, s1, 0);
	}

	uint32_t h0 = s0 ^ HAYA32_KA;
	uint32_t h1 = haya32_rotl(s1, 7) + HAYA32_KB;
	uint32_t h2 = haya32_rotl(s0, 14) ^ HAYA32_KC;
	uint32_t h3 = haya32_rotl(s1, 21) + HAYA32_KD;
	uint32_t w, wp = 0, C = 0;

	if (l >= 128) {
		uint32_t h4 = s1 + HAYA32_KE;
		uint32_t h5 = haya32_rotl(s0, 9) ^ HAYA32_KD;
		uint32_t h6 = haya32_rotl(s1, 18) + HAYA32_KA;
		uint32_t h7 = haya32_rotl(s0, 27) ^ HAYA32_KB;
		do {
			uint32_t t;
			w = haya32_load32(p +  0);
			t = w + haya32_rotl(wp, 11); HAYA32_LANE(h0, t, C); wp = w;
			w = haya32_load32(p +  4);
			t = w + haya32_rotl(wp, 11); HAYA32_LANE(h1, t, C); wp = w;
			w = haya32_load32(p +  8);
			t = w + haya32_rotl(wp, 11); HAYA32_LANE(h2, t, C); wp = w;
			w = haya32_load32(p + 12);
			t = w + haya32_rotl(wp, 11); HAYA32_LANE(h3, t, C); wp = w;
			w = haya32_load32(p + 16);
			t = w + haya32_rotl(wp, 11); HAYA32_LANE(h4, t, C); wp = w;
			w = haya32_load32(p + 20);
			t = w + haya32_rotl(wp, 11); HAYA32_LANE(h5, t, C); wp = w;
			w = haya32_load32(p + 24);
			t = w + haya32_rotl(wp, 11); HAYA32_LANE(h6, t, C); wp = w;
			w = haya32_load32(p + 28);
			t = w + haya32_rotl(wp, 11); HAYA32_LANE(h7, t, C); wp = w;
			h0 += wp;   // per-block raw-word checkpoint
			p += 32; l -= 32;
		} while (l >= 32);
		// 8 -> 4 fold, carry accumulator injected on both sides.
		h0 = (uint32_t)((uint64_t)(h0 ^ haya32_rotl(h4, 11)) *
		     HAYA32_KA) ^ C;
		h1 = (uint32_t)((uint64_t)(h1 ^ haya32_rotl(h5, 19)) *
		     HAYA32_KB);
		h2 = (uint32_t)((uint64_t)(h2 ^ haya32_rotl(h6, 7)) *
		     HAYA32_KC) ^ haya32_rotl(C, 16);
		h3 = (uint32_t)((uint64_t)(h3 ^ haya32_rotl(h7, 23)) *
		     HAYA32_KD);
	}

	// Mid rounds: 4 lanes over 16-byte rounds (used for 9..127-byte
	// inputs and the <32-byte bulk remainder; both stay under the
	// 32-stripe rotl-11 orbit).
	while (l >= 16) {
		uint32_t t;
		w = haya32_load32(p +  0);
		t = w + haya32_rotl(wp, 11); HAYA32_LANE(h0, t, C); wp = w;
		w = haya32_load32(p +  4);
		t = w + haya32_rotl(wp, 11); HAYA32_LANE(h1, t, C); wp = w;
		w = haya32_load32(p +  8);
		t = w + haya32_rotl(wp, 11); HAYA32_LANE(h2, t, C); wp = w;
		w = haya32_load32(p + 12);
		t = w + haya32_rotl(wp, 11); HAYA32_LANE(h3, t, C); wp = w;
		p += 16; l -= 16;
	}

	// Wall: absorb the final stripe's dangling rotated copy.
	h0 += haya32_rotl(wp, 11);

	if (l > 8) {
		h0 = (uint32_t)((uint64_t)(h0 + haya32_inja(haya32_load32(p))) *
		     HAYA32_KA);
		h1 = (uint32_t)((uint64_t)(h1 + haya32_inja(haya32_load32(p + 4))) *
		     HAYA32_KB);
	}
	if (l > 0) {
		h2 = (uint32_t)((uint64_t)(h2 + haya32_injb(haya32_load32(p + l - 8))) *
		     HAYA32_KC);
		h3 = (uint32_t)((uint64_t)(h3 + haya32_injb(haya32_load32(p + l - 4))) *
		     HAYA32_KD);
	}

	return haya32_final(h0, h1, h2, h3, s0, s1, C);
}

#endif // HAYA32_H
