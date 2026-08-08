// haya32x64 - a 64-bit hash built from 32-bit operations.
//
// The algorithm targets JavaScript, wasm fallbacks, RV32, Cortex-M, and
// shader languages: state, additions, xors, and rotations are uint32_t.
// Its only wide primitive is the complete result of a 32x32 multiply.  The
// words API also keeps the seed and digest split, so callers on 32-bit targets
// do not need any other 64-bit arithmetic.
//
// Four structural choices define the hash:
//
//  1. Eight independent lanes absorb 32-byte blocks.  A stripe enters as
//       t = w + rotl32(w_previous, 11).
//     At the first differing stripe w_previous is still equal, so the absorb
//     sequence is injective by induction.  The rotated copy also moves high
//     input differences downward before the next multiply.
//  2. The low product word updates the lane bijectively (the multiplier is
//     odd); the high word, normally discarded by 32-bit hashes, enters a
//     serial carry accumulator.  A raw-word checkpoint once per block breaks
//     rotation-orbit difference ladders.
//  3. The four-lane path is bounded below 128 bytes, fewer than the 32-stripe
//     orbit of rotl-11.  Tails use overlapping little-endian word reads and
//     two distinct bijective three-rotation injections.
//  4. Two Feistel rounds make the seed premix a bijection of all 64 seed bits.
//     The four folded lanes enter the final wide products as operands, avoiding
//     a 32-bit bottleneck before the 64-bit digest.
//
// The digest is defined for lengths 0..UINT32_MAX.  Loads are alignment-safe,
// output is independent of host endianness, and a null key is valid at length
// zero.  This is a non-cryptographic hash; do not use it for authentication or
// with attacker-controlled keys where collision attacks matter.
//
// This is free and unencumbered software released into the public domain.
// For more information, please refer to <https://unlicense.org/>.

#ifndef HAYA32X64_H
#define HAYA32X64_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// A digest, or the complete result of one unsigned 32x32 multiply.
typedef struct haya32x64_words {
	uint32_t lo;
	uint32_t hi;
} haya32x64_words;

// Loads use memcpy (never unaligned or aliasing UB) and swap on known
// big-endian hosts.  Unknown hosts use explicit byte assembly instead of
// silently assuming little endian.
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define HAYA32X64_INTERNAL_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define HAYA32X64_INTERNAL_ENDIAN 0
#elif defined(_WIN32)
#define HAYA32X64_INTERNAL_ENDIAN 0
#else
#define HAYA32X64_INTERNAL_ENDIAN 2
#endif

static inline uint32_t haya32x64_internal_load32le(const uint8_t *p)
{
#if HAYA32X64_INTERNAL_ENDIAN == 2
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
#else
	uint32_t v;
	memcpy(&v, p, sizeof(v));
#if HAYA32X64_INTERNAL_ENDIAN == 1
	v = (v >> 24) | ((v >> 8) & UINT32_C(0x0000FF00)) |
	    ((v & UINT32_C(0x0000FF00)) << 8) | (v << 24);
#endif
	return v;
#endif
}

static inline uint32_t haya32x64_internal_rotl(uint32_t x, int n)
{
	return (x << n) | (x >> (-n & 31));
}

static inline haya32x64_words
haya32x64_internal_mul(uint32_t a, uint32_t b)
{
	uint64_t product = (uint64_t)a * b;
	haya32x64_words result = {
		(uint32_t)product,
		(uint32_t)(product >> 32),
	};
	return result;
}

// Odd 32-bit diffusion constants (from the Murmur3/xxHash families).
#define HAYA32X64_INTERNAL_KA UINT32_C(0x9E3779B1)
#define HAYA32X64_INTERNAL_KB UINT32_C(0x85EBCA77)
#define HAYA32X64_INTERNAL_KC UINT32_C(0xC2B2AE3D)
#define HAYA32X64_INTERNAL_KD UINT32_C(0x27D4EB2F)
#define HAYA32X64_INTERNAL_KE UINT32_C(0x165667B1)
#define HAYA32X64_INTERNAL_F1 UINT32_C(0x85EBCA6B)
#define HAYA32X64_INTERNAL_F2 UINT32_C(0xC2B2AE35)

static inline uint32_t haya32x64_internal_inja(uint32_t w)
{
	return w ^ haya32x64_internal_rotl(w, 10) ^
	       haya32x64_internal_rotl(w, 21);
}

static inline uint32_t haya32x64_internal_injb(uint32_t w)
{
	return w ^ haya32x64_internal_rotl(w, 6) ^
	       haya32x64_internal_rotl(w, 25);
}

// One absorb.  The lane receives the low product half; the high half enters
// the accumulator.  The macro keeps the eight-lane block spelling compact.
#define HAYA32X64_INTERNAL_LANE(h, t, carry) \
	do { \
		haya32x64_words haya32x64_product_ = \
			haya32x64_internal_mul((h) ^ (t), \
			                         HAYA32X64_INTERNAL_KA); \
		(h) = haya32x64_product_.lo; \
		(carry) = haya32x64_internal_rotl((carry), 5) ^ \
		          haya32x64_product_.hi; \
	} while (0)

static inline haya32x64_words
haya32x64_internal_final(uint32_t h0, uint32_t h1,
			 uint32_t h2, uint32_t h3,
			 uint32_t s0, uint32_t s1, uint32_t carry)
{
	h1 ^= carry;
	h3 ^= haya32x64_internal_rotl(carry, 16);
	haya32x64_words m0 = haya32x64_internal_mul(
		h0 ^ s0, h2 ^ HAYA32X64_INTERNAL_KC);
	haya32x64_words m1 = haya32x64_internal_mul(
		h1 ^ HAYA32X64_INTERNAL_KD,
		h3 ^ haya32x64_internal_rotl(s1, 11));
	uint32_t x = m0.lo ^ m1.hi ^ haya32x64_internal_rotl(h1, 7);
	uint32_t y = m0.hi ^ m1.lo ^ haya32x64_internal_rotl(h0, 19);

	// Each cross-avalanche step is bijective on the pair (x, y).
	x ^= x >> 16;
	x *= HAYA32X64_INTERNAL_F1;
	y ^= y >> 15;
	y *= HAYA32X64_INTERNAL_F2;
	x ^= haya32x64_internal_rotl(y, 13);
	y ^= x >> 16;
	x *= HAYA32X64_INTERNAL_KB;
	x ^= x >> 15;
	y *= HAYA32X64_INTERNAL_KE;
	y ^= y >> 14;

	haya32x64_words result = {x, y};
	return result;
}

// Hash bytes with a seed supplied as little-endian 32-bit words.  This is the
// native API for 32-bit targets and mirrors the JavaScript API exactly.
static inline haya32x64_words
haya32x64_hash(const void *key, size_t len, uint32_t seed_lo, uint32_t seed_hi)
{
	const uint8_t *p = (const uint8_t *)key;
	size_t l = len;
	const uint32_t len_word = (uint32_t)len;

	// Two Feistel rounds preserve all 64 bits of the seed.  Earlier per-half
	// lossy folds collided sparse seeds at birthday scale; SMHasher3's
	// SeedSparse test found that prototype flaw.
	haya32x64_words q1 = haya32x64_internal_mul(
		seed_hi ^ HAYA32X64_INTERNAL_KC, HAYA32X64_INTERNAL_KD);
	uint32_t u = seed_lo ^ q1.lo ^ q1.hi;
	haya32x64_words q0 = haya32x64_internal_mul(
		u ^ HAYA32X64_INTERNAL_KA, HAYA32X64_INTERNAL_KB);
	uint32_t v = seed_hi ^ q0.lo ^ q0.hi;
	haya32x64_words q2 = haya32x64_internal_mul(
		len_word + HAYA32X64_INTERNAL_KE, HAYA32X64_INTERNAL_KA);
	uint32_t s0 = u ^ q2.lo;
	uint32_t s1 = v ^ q2.hi;

	if (l <= 8) {
		uint32_t a;
		uint32_t b;
		if (l >= 4) {
			a = haya32x64_internal_load32le(p);
			b = haya32x64_internal_load32le(p + l - 4);
		} else if (l > 0) {
			a = p[0];
			b = ((uint32_t)p[l >> 1] << 8) |
			    ((uint32_t)p[l - 1] << 16);
		} else {
			a = 0;
			b = 0;
		}
		haya32x64_words x = haya32x64_internal_mul(
			haya32x64_internal_inja(a) ^ s0 ^ HAYA32X64_INTERNAL_KB,
			HAYA32X64_INTERNAL_KA);
		haya32x64_words y = haya32x64_internal_mul(
			haya32x64_internal_injb(b) ^
				haya32x64_internal_rotl(s1, 13) ^
				HAYA32X64_INTERNAL_KE,
			HAYA32X64_INTERNAL_KC);
		return haya32x64_internal_final(
			x.lo, x.hi, y.lo, y.hi, s0, s1, 0);
	}

	uint32_t h0 = s0 ^ HAYA32X64_INTERNAL_KA;
	uint32_t h1 = haya32x64_internal_rotl(s1, 7) + HAYA32X64_INTERNAL_KB;
	uint32_t h2 = haya32x64_internal_rotl(s0, 14) ^ HAYA32X64_INTERNAL_KC;
	uint32_t h3 = haya32x64_internal_rotl(s1, 21) + HAYA32X64_INTERNAL_KD;
	uint32_t w;
	uint32_t previous = 0;
	uint32_t carry = 0;

	if (l >= 128) {
		uint32_t h4 = s1 + HAYA32X64_INTERNAL_KE;
		uint32_t h5 = haya32x64_internal_rotl(s0, 9) ^ HAYA32X64_INTERNAL_KD;
		uint32_t h6 = haya32x64_internal_rotl(s1, 18) + HAYA32X64_INTERNAL_KA;
		uint32_t h7 = haya32x64_internal_rotl(s0, 27) ^ HAYA32X64_INTERNAL_KB;
		do {
			uint32_t t;
			w = haya32x64_internal_load32le(p + 0);
			t = w + haya32x64_internal_rotl(previous, 11);
			HAYA32X64_INTERNAL_LANE(h0, t, carry);
			previous = w;
			w = haya32x64_internal_load32le(p + 4);
			t = w + haya32x64_internal_rotl(previous, 11);
			HAYA32X64_INTERNAL_LANE(h1, t, carry);
			previous = w;
			w = haya32x64_internal_load32le(p + 8);
			t = w + haya32x64_internal_rotl(previous, 11);
			HAYA32X64_INTERNAL_LANE(h2, t, carry);
			previous = w;
			w = haya32x64_internal_load32le(p + 12);
			t = w + haya32x64_internal_rotl(previous, 11);
			HAYA32X64_INTERNAL_LANE(h3, t, carry);
			previous = w;
			w = haya32x64_internal_load32le(p + 16);
			t = w + haya32x64_internal_rotl(previous, 11);
			HAYA32X64_INTERNAL_LANE(h4, t, carry);
			previous = w;
			w = haya32x64_internal_load32le(p + 20);
			t = w + haya32x64_internal_rotl(previous, 11);
			HAYA32X64_INTERNAL_LANE(h5, t, carry);
			previous = w;
			w = haya32x64_internal_load32le(p + 24);
			t = w + haya32x64_internal_rotl(previous, 11);
			HAYA32X64_INTERNAL_LANE(h6, t, carry);
			previous = w;
			w = haya32x64_internal_load32le(p + 28);
			t = w + haya32x64_internal_rotl(previous, 11);
			HAYA32X64_INTERNAL_LANE(h7, t, carry);
			previous = w;
			h0 += previous;
			p += 32;
			l -= 32;
		} while (l >= 32);

		h0 = haya32x64_internal_mul(
			h0 ^ haya32x64_internal_rotl(h4, 11),
			HAYA32X64_INTERNAL_KA).lo ^ carry;
		h1 = haya32x64_internal_mul(
			h1 ^ haya32x64_internal_rotl(h5, 19),
			HAYA32X64_INTERNAL_KB).lo;
		h2 = haya32x64_internal_mul(
			h2 ^ haya32x64_internal_rotl(h6, 7),
			HAYA32X64_INTERNAL_KC).lo ^
		     haya32x64_internal_rotl(carry, 16);
		h3 = haya32x64_internal_mul(
			h3 ^ haya32x64_internal_rotl(h7, 23),
			HAYA32X64_INTERNAL_KD).lo;
	}

	// This path sees at most 31 stripes before the final tail, staying below
	// the 32-stripe orbit of the absorb rotation.
	while (l >= 16) {
		uint32_t t;
		w = haya32x64_internal_load32le(p + 0);
		t = w + haya32x64_internal_rotl(previous, 11);
		HAYA32X64_INTERNAL_LANE(h0, t, carry);
		previous = w;
		w = haya32x64_internal_load32le(p + 4);
		t = w + haya32x64_internal_rotl(previous, 11);
		HAYA32X64_INTERNAL_LANE(h1, t, carry);
		previous = w;
		w = haya32x64_internal_load32le(p + 8);
		t = w + haya32x64_internal_rotl(previous, 11);
		HAYA32X64_INTERNAL_LANE(h2, t, carry);
		previous = w;
		w = haya32x64_internal_load32le(p + 12);
		t = w + haya32x64_internal_rotl(previous, 11);
		HAYA32X64_INTERNAL_LANE(h3, t, carry);
		previous = w;
		p += 16;
		l -= 16;
	}

	// Absorb the final stripe's dangling rotated copy.
	h0 += haya32x64_internal_rotl(previous, 11);

	if (l > 8) {
		h0 = haya32x64_internal_mul(
			h0 + haya32x64_internal_inja(
				haya32x64_internal_load32le(p)),
			HAYA32X64_INTERNAL_KA).lo;
		h1 = haya32x64_internal_mul(
			h1 + haya32x64_internal_inja(
				haya32x64_internal_load32le(p + 4)),
			HAYA32X64_INTERNAL_KB).lo;
	}
	if (l > 0) {
		h2 = haya32x64_internal_mul(
			h2 + haya32x64_internal_injb(
				haya32x64_internal_load32le(p + l - 8)),
			HAYA32X64_INTERNAL_KC).lo;
		h3 = haya32x64_internal_mul(
			h3 + haya32x64_internal_injb(
				haya32x64_internal_load32le(p + l - 4)),
			HAYA32X64_INTERNAL_KD).lo;
	}

	return haya32x64_internal_final(h0, h1, h2, h3, s0, s1, carry);
}

// Convenience API for C callers that already use uint64_t values.  The core
// above remains word-oriented; these shifts only split/join the API values.
static inline uint64_t haya32x64(const void *key, size_t len, uint64_t seed)
{
	haya32x64_words digest = haya32x64_hash(
		key, len, (uint32_t)seed, (uint32_t)(seed >> 32));
	return ((uint64_t)digest.hi << 32) | digest.lo;
}

#endif // HAYA32X64_H
