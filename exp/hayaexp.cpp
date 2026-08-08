/*
 * Experimental hayahash variants for SMHasher3 conformance runs.
 * Public domain under The Unlicense, like hayahash itself.
 *
 *   hayastream64 - hayahash64 with the length term moved from the
 *                  premix to the finalizer, making an init/update/
 *                  final API with identical digests possible.
 *   hayahash128  - 128-bit output; low word == hayahash64, high word
 *                  extracted through an algebraically mixed second
 *                  path from the same state.
 *   haya32x64    - 64-bit digest, every operation 32-bit (only
 *                  32x32->64 multiplies): for JS / GPU / RV32 /
 *                  Cortex-M class targets.
 *
 * Generic dispatch spelling only: dispatch shape never changes
 * digests, and these translation units exist for quality testing.
 */

#include "Platform.h"
#include "Hashlib.h"

//------------------------------------------------------------
// Shared 64-bit internals (generic spelling of hayahash64's core).

static inline uint64_t hx_rotl64( uint64_t x, int n ) { return ROTL64(x, n); }

#define HX_K  UINT64_C(0x9E3779B97F4A7C15)
#define HX_M1 UINT64_C(0x3C79AC492BA7B653)
#define HX_M2 UINT64_C(0x1C69B3F74AC4AE35)
#define HX_N1 UINT64_C(0xFF51AFD7ED558CCD)
#define HX_N2 UINT64_C(0xC4CEB9FE1A85EC53)

static inline uint64_t hx_fmix( uint64_t x ) {
    x ^= x >> 27; x *= HX_M1;
    x ^= x >> 33; x *= HX_M2;
    x ^= x >> 27;
    return x;
}

static inline uint64_t hx_fmix2( uint64_t x ) {
    x ^= x >> 30; x *= HX_N1;
    x ^= x >> 31; x *= HX_N2;
    x ^= x >> 33;
    return x;
}

static inline uint64_t hx_long_fmix( uint64_t x, uint64_t K ) {
    x ^= x >> 37;
    x *= K;
    x ^= x >> 32;
    return x;
}

static inline uint64_t hx_inj( uint64_t w ) {
    return w ^ hx_rotl64(w, 21) ^ hx_rotl64(w, 41);
}

static inline uint64_t hx_inj2( uint64_t w ) {
    return w ^ hx_rotl64(w, 11) ^ hx_rotl64(w, 50);
}

#define HX_BULK_BLOCK(q) do {                       \
        w = GET_U64<bswap>((q), 0);                 \
        h0 = (h0 ^ (w + hx_rotl64(wp, 27))) * K;    \
        wp = w;                                     \
        w = GET_U64<bswap>((q), 8);                 \
        h1 = (h1 ^ (w + hx_rotl64(wp, 27))) * K;    \
        wp = w;                                     \
        w = GET_U64<bswap>((q), 16);                \
        h2 = (h2 ^ (w + hx_rotl64(wp, 27))) * K;    \
        wp = w;                                     \
        w = GET_U64<bswap>((q), 24);                \
        h3 = (h3 ^ (w + hx_rotl64(wp, 27))) * K;    \
        wp = w;                                     \
        w = GET_U64<bswap>((q), 32);                \
        h4 = (h4 ^ (w + hx_rotl64(wp, 27))) * K;    \
        wp = w;                                     \
        w = GET_U64<bswap>((q), 40);                \
        h5 = (h5 ^ (w + hx_rotl64(wp, 27))) * K;    \
        wp = w;                                     \
        w = GET_U64<bswap>((q), 48);                \
        h6 = (h6 ^ (w + hx_rotl64(wp, 27))) * K;    \
        wp = w;                                     \
        w = GET_U64<bswap>((q), 56);                \
        h7 = (h7 ^ (w + hx_rotl64(wp, 27))) * K;    \
        wp = w;                                     \
        h0 += wp;                                   \
    } while (0)

// Core of hayastream64 (STREAMLEN = 1) and hayahash128's shared state
// walk. Both produce (s, t0, t1) plus the short-path pre-image.
enum { hx_bulk_min = 320 };

//------------------------------------------------------------
// hayastream64: one-shot spelling of the streaming digest.

template <bool bswap>
static inline uint64_t hayastream64_impl( const uint8_t * p, size_t len, uint64_t seed ) {
    size_t   l = len;
    uint64_t K = HX_K;
    const uint64_t lenmix = (uint64_t)len * K;
    uint64_t s = seed ^ K;

    if (l <= 16) {
        uint64_t a, b;
        if (l >= 8) {
            a = GET_U64<bswap>(p, 0);
            b = GET_U64<bswap>(p + l - 8, 0);
        } else if (l >= 4) {
            a = GET_U32<bswap>(p, 0);
            b = GET_U32<bswap>(p + l - 4, 0);
        } else if (l > 0) {
            a = p[0];
            b = ((uint64_t)p[l >> 1] << 8) | ((uint64_t)p[l - 1] << 16);
        } else {
            a = 0; b = 0;
        }
        uint64_t x = (hx_inj(a) ^ s ^ K) * K;
        uint64_t y = (hx_inj2(b) ^ hx_rotl64(s, 23) ^ (K >> 19)) * HX_M1;
        return hx_fmix(hx_rotl64(x, 27) ^ y ^ lenmix);
    }

    uint64_t h0 = s ^ K;
    uint64_t h1 = hx_rotl64(s, 17) + (K << 21);
    uint64_t h2 = hx_rotl64(s, 34) ^ (K >> 13);
    uint64_t h3 = hx_rotl64(s, 51) + (K << 42);
    uint64_t w, wp = 0;

    if (l >= (size_t)hx_bulk_min) {
        uint64_t h4 = s + (K >> 27);
        uint64_t h5 = hx_rotl64(s, 13) ^ (K << 9);
        uint64_t h6 = hx_rotl64(s, 26) + (K >> 40);
        uint64_t h7 = hx_rotl64(s, 39) ^ (K << 30);
        do {
            HX_BULK_BLOCK(p);
            p += 64; l -= 64;
        } while (l >= 64);
        h0 = (h0 ^ hx_rotl64(h4, 11)) * K;
        h1 = (h1 ^ hx_rotl64(h5, 19)) * K;
        h2 = (h2 ^ hx_rotl64(h6, 31)) * K;
        h3 = (h3 ^ hx_rotl64(h7, 47)) * K;
        if (l >= 32) {
            w = GET_U64<bswap>(p, 0);
            h0 = (h0 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 8);
            h1 = (h1 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 16);
            h2 = (h2 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 24);
            h3 = (h3 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            p += 32; l -= 32;
        }
    } else {
        while (l >= 32) {
            w = GET_U64<bswap>(p, 0);
            h0 = (h0 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 8);
            h1 = (h1 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 16);
            h2 = (h2 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 24);
            h3 = (h3 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            p += 32; l -= 32;
        }
    }

    h0 += hx_rotl64(wp, 27);
    if (l > 16) {
        h0 = (h0 + hx_inj(GET_U64<bswap>(p, 0))) * K;
        h1 = (h1 + hx_inj(GET_U64<bswap>(p, 8))) * K;
    }
    if (l > 0) {
        h2 = (h2 + hx_inj(GET_U64<bswap>(p + l - 16, 0))) * K;
        h3 = (h3 + hx_inj(GET_U64<bswap>(p + l - 8, 0))) * K;
    }
    uint64_t t0 = (h0 ^ hx_rotl64(h1, 13) ^ lenmix) * K;
    uint64_t t1 = (h2 ^ hx_rotl64(h3, 33)) * K;
    uint64_t x  = s ^ t0 ^ hx_rotl64(t1, 29);
    return hx_long_fmix(x, K);
}

//------------------------------------------------------------
// hayahash128: low word == hayahash64.

template <bool bswap>
static inline void hayahash128_impl( const uint8_t * p, size_t len, uint64_t seed,
                                     uint64_t * rlo, uint64_t * rhi ) {
    size_t   l = len;
    uint64_t K = HX_K;
    uint64_t s = seed ^ ((uint64_t)len * K);

    if (l <= 16) {
        uint64_t a, b;
        if (l >= 8) {
            a = GET_U64<bswap>(p, 0);
            b = GET_U64<bswap>(p + l - 8, 0);
        } else if (l >= 4) {
            a = GET_U32<bswap>(p, 0);
            b = GET_U32<bswap>(p + l - 4, 0);
        } else if (l > 0) {
            a = p[0];
            b = ((uint64_t)p[l >> 1] << 8) | ((uint64_t)p[l - 1] << 16);
        } else {
            a = 0; b = 0;
        }
        uint64_t x = (hx_inj(a) ^ s ^ K) * K;
        uint64_t y = (hx_inj2(b) ^ hx_rotl64(s, 23) ^ (K >> 19)) * HX_M1;
        uint64_t u = hx_rotl64(x, 27) ^ y;
        *rlo = hx_fmix(u);
        *rhi = hx_fmix2(x + hx_rotl64(u, 32));
        return;
    }

    uint64_t h0 = s ^ K;
    uint64_t h1 = hx_rotl64(s, 17) + (K << 21);
    uint64_t h2 = hx_rotl64(s, 34) ^ (K >> 13);
    uint64_t h3 = hx_rotl64(s, 51) + (K << 42);
    uint64_t w, wp = 0;

    if (l >= (size_t)hx_bulk_min) {
        uint64_t h4 = s + (K >> 27);
        uint64_t h5 = hx_rotl64(s, 13) ^ (K << 9);
        uint64_t h6 = hx_rotl64(s, 26) + (K >> 40);
        uint64_t h7 = hx_rotl64(s, 39) ^ (K << 30);
        do {
            HX_BULK_BLOCK(p);
            p += 64; l -= 64;
        } while (l >= 64);
        h0 = (h0 ^ hx_rotl64(h4, 11)) * K;
        h1 = (h1 ^ hx_rotl64(h5, 19)) * K;
        h2 = (h2 ^ hx_rotl64(h6, 31)) * K;
        h3 = (h3 ^ hx_rotl64(h7, 47)) * K;
        if (l >= 32) {
            w = GET_U64<bswap>(p, 0);
            h0 = (h0 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 8);
            h1 = (h1 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 16);
            h2 = (h2 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 24);
            h3 = (h3 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            p += 32; l -= 32;
        }
    } else {
        while (l >= 32) {
            w = GET_U64<bswap>(p, 0);
            h0 = (h0 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 8);
            h1 = (h1 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 16);
            h2 = (h2 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            w = GET_U64<bswap>(p, 24);
            h3 = (h3 ^ (w + hx_rotl64(wp, 27))) * K; wp = w;
            p += 32; l -= 32;
        }
    }

    h0 += hx_rotl64(wp, 27);
    if (l > 16) {
        h0 = (h0 + hx_inj(GET_U64<bswap>(p, 0))) * K;
        h1 = (h1 + hx_inj(GET_U64<bswap>(p, 8))) * K;
    }
    if (l > 0) {
        h2 = (h2 + hx_inj(GET_U64<bswap>(p + l - 16, 0))) * K;
        h3 = (h3 + hx_inj(GET_U64<bswap>(p + l - 8, 0))) * K;
    }
    uint64_t t0 = (h0 ^ hx_rotl64(h1, 13)) * K;
    uint64_t t1 = (h2 ^ hx_rotl64(h3, 33)) * K;
    *rlo = hx_long_fmix(s ^ t0 ^ hx_rotl64(t1, 29), K);
    *rhi = hx_fmix2(hx_rotl64(s, 32) ^ (t1 + hx_rotl64(t0, 47)));
}

//------------------------------------------------------------
// haya32x64: 64-bit digest from 32-bit operations only.

static inline uint32_t hy_rotl32( uint32_t x, int n ) { return ROTL32(x, n); }

#define HY_KA 0x9E3779B1u
#define HY_KB 0x85EBCA77u
#define HY_KC 0xC2B2AE3Du
#define HY_KD 0x27D4EB2Fu
#define HY_KE 0x165667B1u
#define HY_F1 0x85EBCA6Bu
#define HY_F2 0xC2B2AE35u

static inline uint32_t hy_inja( uint32_t w ) {
    return w ^ hy_rotl32(w, 10) ^ hy_rotl32(w, 21);
}

static inline uint32_t hy_injb( uint32_t w ) {
    return w ^ hy_rotl32(w, 6) ^ hy_rotl32(w, 25);
}

#define HY_LANE(h, t, C) do {                                   \
        uint64_t p64_ = (uint64_t)((h) ^ (t)) * HY_KA;          \
        (h) = (uint32_t)p64_;                                   \
        (C) = hy_rotl32((C), 5) ^ (uint32_t)(p64_ >> 32);       \
    } while (0)

static inline uint64_t hy_final( uint32_t h0, uint32_t h1, uint32_t h2, uint32_t h3,
                                 uint32_t s0, uint32_t s1, uint32_t C ) {
    h1 ^= C;
    h3 ^= hy_rotl32(C, 16);
    uint64_t m0 = (uint64_t)(h0 ^ s0) * (h2 ^ HY_KC);
    uint64_t m1 = (uint64_t)(h1 ^ HY_KD) * (h3 ^ hy_rotl32(s1, 11));
    uint32_t x = (uint32_t)m0 ^ (uint32_t)(m1 >> 32) ^ hy_rotl32(h1, 7);
    uint32_t y = (uint32_t)(m0 >> 32) ^ (uint32_t)m1 ^ hy_rotl32(h0, 19);
    x ^= x >> 16; x *= HY_F1;
    y ^= y >> 15; y *= HY_F2;
    x ^= hy_rotl32(y, 13);
    y ^= x >> 16;
    x *= HY_KB;
    x ^= x >> 15;
    y *= HY_KE;
    y ^= y >> 14;
    return ((uint64_t)y << 32) | x;
}

template <bool bswap>
static inline uint64_t haya32x64_impl( const uint8_t * p, size_t len, uint64_t seed ) {
    size_t l = len;
    const uint32_t slo = (uint32_t)seed, shi = (uint32_t)(seed >> 32);
    const uint32_t lw  = (uint32_t)len;

    // Feistel premix: (s0, s1) is a bijection of the 64-bit seed.
    uint64_t q1 = (uint64_t)(shi ^ HY_KC) * HY_KD;
    uint32_t u  = slo ^ (uint32_t)q1 ^ (uint32_t)(q1 >> 32);
    uint64_t q0 = (uint64_t)(u ^ HY_KA) * HY_KB;
    uint32_t v  = shi ^ (uint32_t)q0 ^ (uint32_t)(q0 >> 32);
    uint64_t q2 = (uint64_t)(lw + HY_KE) * HY_KA;
    uint32_t s0 = u ^ (uint32_t)q2;
    uint32_t s1 = v ^ (uint32_t)(q2 >> 32);

    if (l <= 8) {
        uint32_t a, b;
        if (l >= 4) {
            a = GET_U32<bswap>(p, 0);
            b = GET_U32<bswap>(p + l - 4, 0);
        } else if (l > 0) {
            a = p[0];
            b = ((uint32_t)p[l >> 1] << 8) | ((uint32_t)p[l - 1] << 16);
        } else {
            a = 0; b = 0;
        }
        uint64_t x64 = (uint64_t)(hy_inja(a) ^ s0 ^ HY_KB) * HY_KA;
        uint64_t y64 = (uint64_t)(hy_injb(b) ^ hy_rotl32(s1, 13) ^ HY_KE) * HY_KC;
        return hy_final((uint32_t)x64, (uint32_t)(x64 >> 32),
                        (uint32_t)y64, (uint32_t)(y64 >> 32), s0, s1, 0);
    }

    uint32_t h0 = s0 ^ HY_KA;
    uint32_t h1 = hy_rotl32(s1, 7) + HY_KB;
    uint32_t h2 = hy_rotl32(s0, 14) ^ HY_KC;
    uint32_t h3 = hy_rotl32(s1, 21) + HY_KD;
    uint32_t w, wp = 0, C = 0;

    if (l >= 128) {
        uint32_t h4 = s1 + HY_KE;
        uint32_t h5 = hy_rotl32(s0, 9) ^ HY_KD;
        uint32_t h6 = hy_rotl32(s1, 18) + HY_KA;
        uint32_t h7 = hy_rotl32(s0, 27) ^ HY_KB;
        do {
            uint32_t t;
            w = GET_U32<bswap>(p, 0);
            t = w + hy_rotl32(wp, 11); HY_LANE(h0, t, C); wp = w;
            w = GET_U32<bswap>(p, 4);
            t = w + hy_rotl32(wp, 11); HY_LANE(h1, t, C); wp = w;
            w = GET_U32<bswap>(p, 8);
            t = w + hy_rotl32(wp, 11); HY_LANE(h2, t, C); wp = w;
            w = GET_U32<bswap>(p, 12);
            t = w + hy_rotl32(wp, 11); HY_LANE(h3, t, C); wp = w;
            w = GET_U32<bswap>(p, 16);
            t = w + hy_rotl32(wp, 11); HY_LANE(h4, t, C); wp = w;
            w = GET_U32<bswap>(p, 20);
            t = w + hy_rotl32(wp, 11); HY_LANE(h5, t, C); wp = w;
            w = GET_U32<bswap>(p, 24);
            t = w + hy_rotl32(wp, 11); HY_LANE(h6, t, C); wp = w;
            w = GET_U32<bswap>(p, 28);
            t = w + hy_rotl32(wp, 11); HY_LANE(h7, t, C); wp = w;
            h0 += wp;
            p += 32; l -= 32;
        } while (l >= 32);
        h0 = (uint32_t)((uint64_t)(h0 ^ hy_rotl32(h4, 11)) * HY_KA) ^ C;
        h1 = (uint32_t)((uint64_t)(h1 ^ hy_rotl32(h5, 19)) * HY_KB);
        h2 = (uint32_t)((uint64_t)(h2 ^ hy_rotl32(h6, 7)) * HY_KC) ^ hy_rotl32(C, 16);
        h3 = (uint32_t)((uint64_t)(h3 ^ hy_rotl32(h7, 23)) * HY_KD);
    }

    while (l >= 16) {
        uint32_t t;
        w = GET_U32<bswap>(p, 0);
        t = w + hy_rotl32(wp, 11); HY_LANE(h0, t, C); wp = w;
        w = GET_U32<bswap>(p, 4);
        t = w + hy_rotl32(wp, 11); HY_LANE(h1, t, C); wp = w;
        w = GET_U32<bswap>(p, 8);
        t = w + hy_rotl32(wp, 11); HY_LANE(h2, t, C); wp = w;
        w = GET_U32<bswap>(p, 12);
        t = w + hy_rotl32(wp, 11); HY_LANE(h3, t, C); wp = w;
        p += 16; l -= 16;
    }

    h0 += hy_rotl32(wp, 11);

    if (l > 8) {
        h0 = (uint32_t)((uint64_t)(h0 + hy_inja(GET_U32<bswap>(p, 0))) * HY_KA);
        h1 = (uint32_t)((uint64_t)(h1 + hy_inja(GET_U32<bswap>(p, 4))) * HY_KB);
    }
    if (l > 0) {
        h2 = (uint32_t)((uint64_t)(h2 + hy_injb(GET_U32<bswap>(p + l - 8, 0))) * HY_KC);
        h3 = (uint32_t)((uint64_t)(h3 + hy_injb(GET_U32<bswap>(p + l - 4, 0))) * HY_KD);
    }

    return hy_final(h0, h1, h2, h3, s0, s1, C);
}

//------------------------------------------------------------

template <bool bswap>
static void HayaStream64( const void * in, const size_t len, const seed_t seed, void * out ) {
    const uint64_t h = hayastream64_impl<bswap>((const uint8_t *)in, len, (uint64_t)seed);
    PUT_U64<bswap>(h, (uint8_t *)out, 0);
}

template <bool bswap>
static void HayaHash128( const void * in, const size_t len, const seed_t seed, void * out ) {
    uint64_t lo, hi;
    hayahash128_impl<bswap>((const uint8_t *)in, len, (uint64_t)seed, &lo, &hi);
    PUT_U64<bswap>(lo, (uint8_t *)out, 0);
    PUT_U64<bswap>(hi, (uint8_t *)out, 8);
}

template <bool bswap>
static void Haya32x64( const void * in, const size_t len, const seed_t seed, void * out ) {
    const uint64_t h = haya32x64_impl<bswap>((const uint8_t *)in, len, (uint64_t)seed);
    PUT_U64<bswap>(h, (uint8_t *)out, 0);
}

REGISTER_FAMILY(hayaexp,
   $.src_url    = "https://github.com/thevilledev/hayahash",
   $.src_status = HashFamilyInfo::SRC_ACTIVE
 );

REGISTER_HASH(hayastream64,
   $.desc            = "hayahash64 streaming-capable variant (length in finalizer)",
   $.hash_flags      =
         FLAG_HASH_ENDIAN_INDEPENDENT,
   $.impl_flags      =
         FLAG_IMPL_CANONICAL_LE          |
         FLAG_IMPL_LICENSE_PUBLIC_DOMAIN |
         FLAG_IMPL_MULTIPLY_64_64        |
         FLAG_IMPL_ROTATE,
   $.bits            = 64,
   $.verification_LE = 0x65F2AC15,
   $.verification_BE = 0x805DE5C0,
   $.hashfn_native   = HayaStream64<false>,
   $.hashfn_bswap    = HayaStream64<true>
 );

REGISTER_HASH(hayahash128,
   $.desc            = "hayahash 128-bit variant (low word == hayahash64)",
   $.hash_flags      =
         FLAG_HASH_ENDIAN_INDEPENDENT,
   $.impl_flags      =
         FLAG_IMPL_CANONICAL_LE          |
         FLAG_IMPL_LICENSE_PUBLIC_DOMAIN |
         FLAG_IMPL_MULTIPLY_64_64        |
         FLAG_IMPL_ROTATE,
   $.bits            = 128,
   $.verification_LE = 0xF208F8BE,
   $.verification_BE = 0x666BE786,
   $.hashfn_native   = HayaHash128<false>,
   $.hashfn_bswap    = HayaHash128<true>
 );

REGISTER_HASH(haya32x64,
   $.desc            = "64-bit digest from 32-bit ops only (32x32->64 multiplies)",
   $.hash_flags      =
         FLAG_HASH_ENDIAN_INDEPENDENT,
   $.impl_flags      =
         FLAG_IMPL_CANONICAL_LE          |
         FLAG_IMPL_LICENSE_PUBLIC_DOMAIN |
         FLAG_IMPL_MULTIPLY               |
         FLAG_IMPL_ROTATE,
   $.bits            = 64,
   $.verification_LE = 0x7137E6DC,
   $.verification_BE = 0x1DFB2C55,
   $.hashfn_native   = Haya32x64<false>,
   $.hashfn_bswap    = Haya32x64<true>
 );
