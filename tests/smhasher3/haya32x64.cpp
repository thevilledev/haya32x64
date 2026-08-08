/*
 * SMHasher3 adapter for haya32x64.
 * Public domain under The Unlicense, like haya32x64 itself.
 */

#include "Platform.h"
#include "Hashlib.h"

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

static inline uint64_t hy_final( uint32_t h0, uint32_t h1,
                                 uint32_t h2, uint32_t h3,
                                 uint32_t s0, uint32_t s1,
                                 uint32_t C, uint32_t len ) {
    h1 ^= C;
    h3 ^= hy_rotl32(C, 16);
    uint64_t lm = (uint64_t)(len + HY_KE) * HY_KA;
    uint64_t m0 = (uint64_t)(h0 ^ s0 ^ (uint32_t)lm) * (h2 ^ HY_KC);
    uint64_t m1 = (uint64_t)(h1 ^ HY_KD) *
                  (h3 ^ hy_rotl32(s1, 11) ^ (uint32_t)(lm >> 32));
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
static inline uint64_t haya32x64_impl( const uint8_t * p, size_t len,
                                       uint64_t seed ) {
    size_t l = len;
    const uint32_t slo = (uint32_t)seed;
    const uint32_t shi = (uint32_t)(seed >> 32);
    const uint32_t lw  = (uint32_t)len;

    uint64_t q1 = (uint64_t)(shi ^ HY_KC) * HY_KD;
    uint32_t u  = slo ^ (uint32_t)q1 ^ (uint32_t)(q1 >> 32);
    uint64_t q0 = (uint64_t)(u ^ HY_KA) * HY_KB;
    uint32_t v  = shi ^ (uint32_t)q0 ^ (uint32_t)(q0 >> 32);
    uint32_t s0 = u;
    uint32_t s1 = v;

    if (l <= 8) {
        uint32_t a, b;
        if (l >= 4) {
            a = GET_U32<bswap>(p, 0);
            b = GET_U32<bswap>(p + l - 4, 0);
        } else if (l > 0) {
            a = p[0];
            b = ((uint32_t)p[l >> 1] << 8) |
                ((uint32_t)p[l - 1] << 16);
        } else {
            a = 0;
            b = 0;
        }
        uint64_t x64 = (uint64_t)(hy_inja(a) ^ s0 ^ HY_KB) * HY_KA;
        uint64_t y64 = (uint64_t)(hy_injb(b) ^
                                  hy_rotl32(s1, 13) ^ HY_KE) * HY_KC;
        return hy_final((uint32_t)x64, (uint32_t)(x64 >> 32),
                        (uint32_t)y64, (uint32_t)(y64 >> 32),
                        s0, s1, 0, lw);
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
            p += 32;
            l -= 32;
        } while (l >= 32);
        h0 = (uint32_t)((uint64_t)(h0 ^ hy_rotl32(h4, 11)) * HY_KA) ^ C;
        h1 = (uint32_t)((uint64_t)(h1 ^ hy_rotl32(h5, 19)) * HY_KB);
        h2 = (uint32_t)((uint64_t)(h2 ^ hy_rotl32(h6, 7)) * HY_KC) ^
             hy_rotl32(C, 16);
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
        p += 16;
        l -= 16;
    }

    h0 += hy_rotl32(wp, 11);
    if (l > 8) {
        h0 = (uint32_t)((uint64_t)(h0 +
              hy_inja(GET_U32<bswap>(p, 0))) * HY_KA);
        h1 = (uint32_t)((uint64_t)(h1 +
              hy_inja(GET_U32<bswap>(p, 4))) * HY_KB);
    }
    if (l > 0) {
        h2 = (uint32_t)((uint64_t)(h2 +
              hy_injb(GET_U32<bswap>(p + l - 8, 0))) * HY_KC);
        h3 = (uint32_t)((uint64_t)(h3 +
              hy_injb(GET_U32<bswap>(p + l - 4, 0))) * HY_KD);
    }

    return hy_final(h0, h1, h2, h3, s0, s1, C, lw);
}

template <bool bswap>
static void Haya32x64( const void * in, const size_t len,
                       const seed_t seed, void * out ) {
    const uint64_t h = haya32x64_impl<bswap>(
        (const uint8_t *)in, len, (uint64_t)seed);
    PUT_U64<bswap>(h, (uint8_t *)out, 0);
}

REGISTER_FAMILY(haya32x64_family,
   $.src_url    = "https://github.com/thevilledev/haya32x64",
   $.src_status = HashFamilyInfo::SRC_ACTIVE
 );

REGISTER_HASH(haya32x64,
   $.desc            = "64-bit digest from 32-bit ops only (32x32->64 multiplies)",
   $.hash_flags      =
         FLAG_HASH_ENDIAN_INDEPENDENT,
   $.impl_flags      =
         FLAG_IMPL_CANONICAL_LE          |
         FLAG_IMPL_LICENSE_PUBLIC_DOMAIN |
         FLAG_IMPL_MULTIPLY              |
         FLAG_IMPL_ROTATE,
   $.bits            = 64,
   $.verification_LE = 0xEAA8E435,
   $.verification_BE = 0x8705401D,
   $.hashfn_native   = Haya32x64<false>,
   $.hashfn_bswap    = Haya32x64<true>
 );
