// Experimental hayahash variants, built on the unmodified reference
// header (included below for its load/rotl/fmix internals).
//
//  1. hayastream64: a streaming-capable variant. hayahash64 premixes
//     seed AND length into `s` before touching any input, and every
//     lane IV derives from `s` - so a bit-exact streaming API for
//     hayahash64 is impossible without buffering the whole input.
//     This variant moves the length term from the premix to the
//     finalizer (s = seed ^ K; final x ^= len * K), which keeps the
//     cross-length separation argument (len -> len*K is injective,
//     the final fmix is bijective) while making the digest a pure
//     function of (seed, bytes-so-far) - exactly what init/update/
//     final needs. Everything else is hayahash64 unchanged.
//
//  2. hayahash128: 128-bit output. The low word is derived exactly as
//     hayahash64 computes its digest; the high word is extracted from
//     the same internal state through a second, algebraically mixed
//     path:
//       - short path (<= 16B): the pair (a', b') = (rotl(x,27) ^ y, x)
//         is a bijection of the 128-bit pre-image (x, y), so the pair
//         of outputs (fmix(a'), fmix2(b' + rotl(a',32))) is also a
//         bijection: 128-bit collisions on the short path require a
//         collision in (x, y) itself.
//       - mid/long paths: low = long_fmix(s ^ t0 ^ rotl(t1,29)) as in
//         hayahash64; high = long_fmix2(rotl(s,32) ^ (t1 + rotl(t0,47))).
//         The high word combines t0/t1 with + where the low word used
//         ^, so no GF(2)-linear kernel can cancel a (dt0, dt1)
//         difference in both words simultaneously.
//
// Both are experiments; digests are frozen only for the differential
// tests in this directory.

#ifndef HAYAVARIANTS_H
#define HAYAVARIANTS_H

#include "hayahash.h"

// ------------------------------------------------------------------
// Shared second-finalizer constants (distinct from K/M1/M2 so the two
// 128-bit output words decorrelate; these are the murmur3 finalizer
// constants, odd and well tested).
#define HAYA128_N1 UINT64_C(0xFF51AFD7ED558CCD)
#define HAYA128_N2 UINT64_C(0xC4CEB9FE1A85EC53)

static inline uint64_t haya_internal_fmix2(uint64_t x)
{
	x ^= x >> 30; x *= HAYA128_N1;
	x ^= x >> 31; x *= HAYA128_N2;
	x ^= x >> 33;
	return x;
}

// ==================================================================
// 1. hayastream64
// ==================================================================

// One-shot spelling: hayahash64 with the length term moved from the
// premix to the finalizer. Generic dispatch only (compact shape); the
// point here is digest definition + quality, not per-arch tuning.
static inline uint64_t
hayastream64_oneshot(const void *keyIn, ptrdiff_t len, uint64_t seed)
{
	const uint8_t *p = (const uint8_t *)keyIn;
	ptrdiff_t l = len;
	uint64_t K = HAYAHASH64_INTERNAL_K;
#ifdef HAYASTREAM_STRUCTCHECK
	// Test-only: restore the length premix; the function must then be
	// bit-identical to hayahash64, proving the spelling differs only
	// in the intended two length-handling changes.
	const uint64_t lenmix = 0;
	uint64_t s = seed ^ ((uint64_t)len * K);
#else
	const uint64_t lenmix = (uint64_t)len * K;
	uint64_t s = seed ^ K;
#endif

	if (l <= 16) {
		uint64_t a, b;
		if (l >= 8) {
			a = hayahash64_internal_load64le(p);
			b = hayahash64_internal_load64le(p + l - 8);
		} else if (l >= 4) {
			a = hayahash64_internal_load32le(p);
			b = hayahash64_internal_load32le(p + l - 4);
		} else if (l > 0) {
			a = p[0];
			b = ((uint64_t)p[l >> 1] << 8) | ((uint64_t)p[l - 1] << 16);
		} else {
			a = 0; b = 0;
		}
		uint64_t x = (hayahash64_internal_inj(a) ^ s ^ K) * K;
		uint64_t y = (hayahash64_internal_inj2(b) ^ hayahash64_internal_rotl(s, 23) ^
		              (K >> 19)) * HAYAHASH64_INTERNAL_M1;
		return hayahash64_internal_fmix(
			hayahash64_internal_rotl_product(x, 27) ^ y ^ lenmix);
	}

	uint64_t h0 = s ^ K;
	uint64_t h1 = hayahash64_internal_rotl(s, 17) + (K << 21);
	uint64_t h2 = hayahash64_internal_rotl(s, 34) ^ (K >> 13);
	uint64_t h3 = hayahash64_internal_rotl(s, 51) + (K << 42);
	uint64_t w, wp = 0;

	if (l >= hayahash64_internal_bulk_min) {
		uint64_t h4 = s + (K >> 27);
		uint64_t h5 = hayahash64_internal_rotl(s, 13) ^ (K << 9);
		uint64_t h6 = hayahash64_internal_rotl(s, 26) + (K >> 40);
		uint64_t h7 = hayahash64_internal_rotl(s, 39) ^ (K << 30);
		do {
			HAYAHASH64_INTERNAL_BULK_BLOCK(p);
			p += 64; l -= 64;
		} while (l >= 64);
		h0 = (h0 ^ hayahash64_internal_rotl_product(h4, 11)) * K;
		h1 = (h1 ^ hayahash64_internal_rotl_product(h5, 19)) * K;
		h2 = (h2 ^ hayahash64_internal_rotl_product(h6, 31)) * K;
		h3 = (h3 ^ hayahash64_internal_rotl_product(h7, 47)) * K;
	} else {
		while (l >= 32) {
			w = hayahash64_internal_load64le(p +  0);
			h0 = (h0 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			w = hayahash64_internal_load64le(p +  8);
			h1 = (h1 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			w = hayahash64_internal_load64le(p + 16);
			h2 = (h2 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			w = hayahash64_internal_load64le(p + 24);
			h3 = (h3 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			p += 32; l -= 32;
		}
		goto tail;
	}

	if (l >= 32) {
		w = hayahash64_internal_load64le(p +  0);
		h0 = (h0 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
		wp = w;
		w = hayahash64_internal_load64le(p +  8);
		h1 = (h1 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
		wp = w;
		w = hayahash64_internal_load64le(p + 16);
		h2 = (h2 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
		wp = w;
		w = hayahash64_internal_load64le(p + 24);
		h3 = (h3 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
		wp = w;
		p += 32; l -= 32;
	}

tail:
	h0 += hayahash64_internal_rotl(wp, 27);
	if (l > 16) {
		h0 = (h0 + hayahash64_internal_injp(p + 0)) * K;
		h1 = (h1 + hayahash64_internal_injp(p + 8)) * K;
	}
	if (l > 0) {
		h2 = (h2 + hayahash64_internal_injp(p + l - 16)) * K;
		h3 = (h3 + hayahash64_internal_injp(p + l -  8)) * K;
	}
	{
		// lenmix must pass through a multiply against state: xoring
		// it after the folds left mod-2^64 low-bit structure across
		// lengths of identical-state (all-zero) keys - found by
		// SMHasher3 SeedZeroes differentials.
		uint64_t t0 = (h0 ^ hayahash64_internal_rotl_product(h1, 13) ^
		               lenmix) * K;
		uint64_t t1 = (h2 ^ hayahash64_internal_rotl_product(h3, 33)) * K;
		uint64_t x = s ^ t0 ^ hayahash64_internal_rotl_product(t1, 29);
		return hayahash64_internal_long_fmix(x, K);
	}
}

// ---------------------------------------------------------- streaming
// State machine. Buffers up to 383 bytes before committing to the
// 8-lane bulk path (the one-shot dispatch boundary is 320; totals in
// [320,383] can still be finished one-shot from the buffer). In bulk
// mode the buffer never drops below 128 bytes so the digest-time
// mid/tail phase can read its up-to-16-byte reach-back and up-to-63
// byte remainder without touching consumed memory.

enum { hayastream64_bufcap = 448, hayastream64_keep = 128 };

typedef struct {
	uint64_t h[8];
	uint64_t wp;
	uint64_t seed;
	uint64_t total;
	uint32_t nbuf;
	uint32_t bulk;
	uint8_t buf[hayastream64_bufcap];
} hayastream64_state;

static inline void hayastream64_init(hayastream64_state *st, uint64_t seed)
{
	uint64_t K = HAYAHASH64_INTERNAL_K;
	uint64_t s = seed ^ K;
	st->h[0] = s ^ K;
	st->h[1] = hayahash64_internal_rotl(s, 17) + (K << 21);
	st->h[2] = hayahash64_internal_rotl(s, 34) ^ (K >> 13);
	st->h[3] = hayahash64_internal_rotl(s, 51) + (K << 42);
	st->h[4] = s + (K >> 27);
	st->h[5] = hayahash64_internal_rotl(s, 13) ^ (K << 9);
	st->h[6] = hayahash64_internal_rotl(s, 26) + (K >> 40);
	st->h[7] = hayahash64_internal_rotl(s, 39) ^ (K << 30);
	st->wp = 0;
	st->seed = seed;
	st->total = 0;
	st->nbuf = 0;
	st->bulk = 0;
}

// Consume `n` bytes (a multiple of 64) through the bulk block.
static inline void
hayastream64_blocks(hayastream64_state *st, const uint8_t *p, size_t n)
{
	uint64_t K = HAYAHASH64_INTERNAL_K;
	uint64_t h0 = st->h[0], h1 = st->h[1], h2 = st->h[2], h3 = st->h[3];
	uint64_t h4 = st->h[4], h5 = st->h[5], h6 = st->h[6], h7 = st->h[7];
	uint64_t w, wp = st->wp;
	const uint8_t *pe = p + n;
	while (p != pe) {
		HAYAHASH64_INTERNAL_BULK_BLOCK(p);
		p += 64;
	}
	st->h[0] = h0; st->h[1] = h1; st->h[2] = h2; st->h[3] = h3;
	st->h[4] = h4; st->h[5] = h5; st->h[6] = h6; st->h[7] = h7;
	st->wp = wp;
}

// Why `keep` = 128 works: at digest time the leftover buffer is
// consumed in 64-byte blocks until fewer than 64 bytes remain, and
// only then does the mid/tail phase run. Its deepest reach-back is 16
// bytes before the current pointer. With nbuf >= 80 the digest-time
// block loop always consumes at least 64 buffered bytes first, so
// every reach-back address stays inside the buffer; keep = 128 gives
// that invariant two blocks of margin. Consumed bytes are never
// retained: the buffer holds unconsumed stream bytes only.
static inline void
hayastream64_update(hayastream64_state *st, const void *data, size_t n)
{
	const uint8_t *p = (const uint8_t *)data;
	if (n == 0)
		return;
	st->total += n;

	if (!st->bulk) {
		// Still undecided between the one-shot finish and bulk.
		if (st->nbuf + n < (size_t)hayastream64_bufcap) {
			memcpy(st->buf + st->nbuf, p, n);
			st->nbuf += (uint32_t)n;
			return;
		}
		// Total is now >= 448 > 320: commit to the bulk path.
		st->bulk = 1;
	}

	for (;;) {
		// Fast path: buffer at its floor and plenty incoming -
		// drain the buffered floor, then stream full blocks
		// straight from the caller's memory, leaving a
		// [keep, keep+63]-byte remainder for the buffer.
		if (st->nbuf == (uint32_t)hayastream64_keep &&
		    n > (size_t)hayastream64_bufcap) {
			size_t direct = (n - hayastream64_keep) & ~(size_t)63;
			hayastream64_blocks(st, st->buf, hayastream64_keep);
			hayastream64_blocks(st, p, direct);
			p += direct; n -= direct;
			st->nbuf = 0;
			// Fall through: remainder (< bufcap) is buffered below.
		}
		size_t room = (size_t)hayastream64_bufcap - st->nbuf;
		size_t take = n < room ? n : room;
		memcpy(st->buf + st->nbuf, p, take);
		st->nbuf += (uint32_t)take;
		p += take; n -= take;
		if (st->nbuf < (uint32_t)hayastream64_bufcap)
			break;
		// Buffer full: consume whole blocks down to the keep floor.
		size_t consume = (size_t)(st->nbuf - hayastream64_keep) &
		                 ~(size_t)63;
		hayastream64_blocks(st, st->buf, consume);
		st->nbuf -= (uint32_t)consume;
		memmove(st->buf, st->buf + consume, st->nbuf);
	}
}

static inline uint64_t hayastream64_final(const hayastream64_state *st)
{
	uint64_t K = HAYAHASH64_INTERNAL_K;

	if (!st->bulk)
		return hayastream64_oneshot(st->buf, (ptrdiff_t)st->total,
		                            st->seed);

	uint64_t s = st->seed ^ K;
	const uint64_t lenmix = st->total * K;
	uint64_t h0 = st->h[0], h1 = st->h[1], h2 = st->h[2], h3 = st->h[3];
	uint64_t h4 = st->h[4], h5 = st->h[5], h6 = st->h[6], h7 = st->h[7];
	uint64_t w, wp = st->wp;
	const uint8_t *p = st->buf;
	size_t l = st->nbuf;

	while (l >= 64) {
		HAYAHASH64_INTERNAL_BULK_BLOCK(p);
		p += 64; l -= 64;
	}
	h0 = (h0 ^ hayahash64_internal_rotl_product(h4, 11)) * K;
	h1 = (h1 ^ hayahash64_internal_rotl_product(h5, 19)) * K;
	h2 = (h2 ^ hayahash64_internal_rotl_product(h6, 31)) * K;
	h3 = (h3 ^ hayahash64_internal_rotl_product(h7, 47)) * K;

	if (l >= 32) {
		w = hayahash64_internal_load64le(p +  0);
		h0 = (h0 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
		wp = w;
		w = hayahash64_internal_load64le(p +  8);
		h1 = (h1 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
		wp = w;
		w = hayahash64_internal_load64le(p + 16);
		h2 = (h2 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
		wp = w;
		w = hayahash64_internal_load64le(p + 24);
		h3 = (h3 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
		wp = w;
		p += 32; l -= 32;
	}
	h0 += hayahash64_internal_rotl(wp, 27);
	if (l > 16) {
		h0 = (h0 + hayahash64_internal_injp(p + 0)) * K;
		h1 = (h1 + hayahash64_internal_injp(p + 8)) * K;
	}
	if (l > 0) {
		h2 = (h2 + hayahash64_internal_injp(p + l - 16)) * K;
		h3 = (h3 + hayahash64_internal_injp(p + l -  8)) * K;
	}
	uint64_t t0 = (h0 ^ hayahash64_internal_rotl_product(h1, 13) ^
	               lenmix) * K;
	uint64_t t1 = (h2 ^ hayahash64_internal_rotl_product(h3, 33)) * K;
	uint64_t x = s ^ t0 ^ hayahash64_internal_rotl_product(t1, 29);
	return hayahash64_internal_long_fmix(x, K);
}

// ==================================================================
// 2. hayahash128
// ==================================================================

typedef struct { uint64_t lo, hi; } hayahash128_t;

static inline hayahash128_t
hayahash128(const void *keyIn, ptrdiff_t len, uint64_t seed)
{
	const uint8_t *p = (const uint8_t *)keyIn;
	ptrdiff_t l = len;
	uint64_t K = HAYAHASH64_INTERNAL_K;
	uint64_t s = seed ^ ((uint64_t)len * K);
	hayahash128_t r;

	if (l <= 16) {
		uint64_t a, b;
		if (l >= 8) {
			a = hayahash64_internal_load64le(p);
			b = hayahash64_internal_load64le(p + l - 8);
		} else if (l >= 4) {
			a = hayahash64_internal_load32le(p);
			b = hayahash64_internal_load32le(p + l - 4);
		} else if (l > 0) {
			a = p[0];
			b = ((uint64_t)p[l >> 1] << 8) | ((uint64_t)p[l - 1] << 16);
		} else {
			a = 0; b = 0;
		}
		uint64_t x = (hayahash64_internal_inj(a) ^ s ^ K) * K;
		uint64_t y = (hayahash64_internal_inj2(b) ^ hayahash64_internal_rotl(s, 23) ^
		              (K >> 19)) * HAYAHASH64_INTERNAL_M1;
		// (u, x) is a bijection of (x, y); the output pair is a
		// bijection of (u, x): no 128-bit short-path collisions
		// beyond collisions of the pre-image itself.
		uint64_t u = hayahash64_internal_rotl_product(x, 27) ^ y;
		r.lo = hayahash64_internal_fmix(u);
		r.hi = haya_internal_fmix2(x + hayahash64_internal_rotl(u, 32));
		return r;
	}

	uint64_t h0 = s ^ K;
	uint64_t h1 = hayahash64_internal_rotl(s, 17) + (K << 21);
	uint64_t h2 = hayahash64_internal_rotl(s, 34) ^ (K >> 13);
	uint64_t h3 = hayahash64_internal_rotl(s, 51) + (K << 42);
	uint64_t w, wp = 0;

	if (l >= hayahash64_internal_bulk_min) {
		uint64_t h4 = s + (K >> 27);
		uint64_t h5 = hayahash64_internal_rotl(s, 13) ^ (K << 9);
		uint64_t h6 = hayahash64_internal_rotl(s, 26) + (K >> 40);
		uint64_t h7 = hayahash64_internal_rotl(s, 39) ^ (K << 30);
		do {
			HAYAHASH64_INTERNAL_BULK_BLOCK(p);
			p += 64; l -= 64;
		} while (l >= 64);
		h0 = (h0 ^ hayahash64_internal_rotl_product(h4, 11)) * K;
		h1 = (h1 ^ hayahash64_internal_rotl_product(h5, 19)) * K;
		h2 = (h2 ^ hayahash64_internal_rotl_product(h6, 31)) * K;
		h3 = (h3 ^ hayahash64_internal_rotl_product(h7, 47)) * K;
		if (l >= 32) {
			w = hayahash64_internal_load64le(p +  0);
			h0 = (h0 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			w = hayahash64_internal_load64le(p +  8);
			h1 = (h1 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			w = hayahash64_internal_load64le(p + 16);
			h2 = (h2 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			w = hayahash64_internal_load64le(p + 24);
			h3 = (h3 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			p += 32; l -= 32;
		}
	} else {
		while (l >= 32) {
			w = hayahash64_internal_load64le(p +  0);
			h0 = (h0 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			w = hayahash64_internal_load64le(p +  8);
			h1 = (h1 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			w = hayahash64_internal_load64le(p + 16);
			h2 = (h2 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			w = hayahash64_internal_load64le(p + 24);
			h3 = (h3 ^ (w + hayahash64_internal_rotl(wp, 27))) * K;
			wp = w;
			p += 32; l -= 32;
		}
	}

	h0 += hayahash64_internal_rotl(wp, 27);
	if (l > 16) {
		h0 = (h0 + hayahash64_internal_injp(p + 0)) * K;
		h1 = (h1 + hayahash64_internal_injp(p + 8)) * K;
	}
	if (l > 0) {
		h2 = (h2 + hayahash64_internal_injp(p + l - 16)) * K;
		h3 = (h3 + hayahash64_internal_injp(p + l -  8)) * K;
	}
	{
		uint64_t t0 = (h0 ^ hayahash64_internal_rotl_product(h1, 13)) * K;
		uint64_t t1 = (h2 ^ hayahash64_internal_rotl_product(h3, 33)) * K;
		r.lo = hayahash64_internal_long_fmix(
			s ^ t0 ^ hayahash64_internal_rotl_product(t1, 29), K);
		r.hi = haya_internal_fmix2(hayahash64_internal_rotl(s, 32) ^
			(t1 + hayahash64_internal_rotl(t0, 47)));
		return r;
	}
}

#endif // HAYAVARIANTS_H
