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

#define HY_LANES4_SERIAL(h0, h1, h2, h3, t0, t1, t2, t3, C) do { \
        uint64_t p_ = (uint64_t)((h0) ^ (t0)) * HY_KA;         \
        (h0) = (uint32_t)p_;                                   \
        (C) = hy_rotl32((C), 5) ^ (uint32_t)(p_ >> 32);        \
        p_ = (uint64_t)((h1) ^ (t1)) * HY_KA;                  \
        (h1) = (uint32_t)p_;                                   \
        (C) = hy_rotl32((C), 5) ^ (uint32_t)(p_ >> 32);        \
        p_ = (uint64_t)((h2) ^ (t2)) * HY_KA;                  \
        (h2) = (uint32_t)p_;                                   \
        (C) = hy_rotl32((C), 5) ^ (uint32_t)(p_ >> 32);        \
        p_ = (uint64_t)((h3) ^ (t3)) * HY_KA;                  \
        (h3) = (uint32_t)p_;                                   \
        (C) = hy_rotl32((C), 5) ^ (uint32_t)(p_ >> 32);        \
    } while (0)

#define HY_LANES4_GROUPED(h0, h1, h2, h3, t0, t1, t2, t3, C) do { \
        uint64_t p0_ = (uint64_t)((h0) ^ (t0)) * HY_KA;         \
        uint64_t p1_ = (uint64_t)((h1) ^ (t1)) * HY_KA;         \
        uint64_t p2_ = (uint64_t)((h2) ^ (t2)) * HY_KA;         \
        uint64_t p3_ = (uint64_t)((h3) ^ (t3)) * HY_KA;         \
        (h0) = (uint32_t)p0_;                                   \
        (h1) = (uint32_t)p1_;                                   \
        (h2) = (uint32_t)p2_;                                   \
        (h3) = (uint32_t)p3_;                                   \
        (C) = hy_rotl32((C), 20) ^                              \
              hy_rotl32((uint32_t)(p0_ >> 32), 15) ^            \
              hy_rotl32((uint32_t)(p1_ >> 32), 10) ^            \
              hy_rotl32((uint32_t)(p2_ >> 32), 5) ^             \
              (uint32_t)(p3_ >> 32);                            \
    } while (0)

// The paired carry fold is algebraically identical to two serial updates per
// pair while keeping fewer complete products live than the grouped form.
#define HY_LANES4_PAIRED(h0, h1, h2, h3, t0, t1, t2, t3, C) do { \
        uint64_t p0_ = (uint64_t)((h0) ^ (t0)) * HY_KA;        \
        uint64_t p1_ = (uint64_t)((h1) ^ (t1)) * HY_KA;        \
        (h0) = (uint32_t)p0_;                                  \
        (h1) = (uint32_t)p1_;                                  \
        (C) = hy_rotl32((C), 10) ^                             \
              hy_rotl32((uint32_t)(p0_ >> 32), 5) ^            \
              (uint32_t)(p1_ >> 32);                           \
        p0_ = (uint64_t)((h2) ^ (t2)) * HY_KA;                 \
        p1_ = (uint64_t)((h3) ^ (t3)) * HY_KA;                 \
        (h2) = (uint32_t)p0_;                                  \
        (h3) = (uint32_t)p1_;                                  \
        (C) = hy_rotl32((C), 10) ^                             \
              hy_rotl32((uint32_t)(p0_ >> 32), 5) ^            \
              (uint32_t)(p1_ >> 32);                           \
    } while (0)

#if defined(__aarch64__)
#define HY_LANES4_BULK HY_LANES4_PAIRED
#define HY_LANES4_MID HY_LANES4_SERIAL
#else
#define HY_LANES4_BULK HY_LANES4_GROUPED
#define HY_LANES4_MID HY_LANES4_GROUPED
#endif

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

struct hy_bulk_state {
    uint32_t h[8];
    uint32_t previous;
    uint32_t carry;
};

template <bool bswap>
static NEVER_INLINE void hy_bulk( hy_bulk_state & state,
                                  const uint8_t * p, size_t length ) {
    uint32_t h0 = state.h[0];
    uint32_t h1 = state.h[1];
    uint32_t h2 = state.h[2];
    uint32_t h3 = state.h[3];
    uint32_t h4 = state.h[4];
    uint32_t h5 = state.h[5];
    uint32_t h6 = state.h[6];
    uint32_t h7 = state.h[7];
    uint32_t wp = state.previous;
    uint32_t C  = state.carry;
#if defined(__GNUC__) && !defined(__clang__) && defined(__aarch64__)
#pragma GCC unroll 8
#elif defined(__GNUC__) && !defined(__clang__) && \
      (defined(__x86_64__) || defined(__i386__))
#pragma GCC unroll 2
#endif
    while (length != 0) {
        uint32_t w0 = GET_U32<bswap>(p, 0);
        uint32_t w1 = GET_U32<bswap>(p, 4);
        uint32_t w2 = GET_U32<bswap>(p, 8);
        uint32_t w3 = GET_U32<bswap>(p, 12);
        uint32_t t0 = w0 + hy_rotl32(wp, 11);
        uint32_t t1 = w1 + hy_rotl32(w0, 11);
        uint32_t t2 = w2 + hy_rotl32(w1, 11);
        uint32_t t3 = w3 + hy_rotl32(w2, 11);
        HY_LANES4_BULK(h0, h1, h2, h3, t0, t1, t2, t3, C);

        uint32_t w4 = GET_U32<bswap>(p, 16);
        uint32_t w5 = GET_U32<bswap>(p, 20);
        uint32_t w6 = GET_U32<bswap>(p, 24);
        uint32_t w7 = GET_U32<bswap>(p, 28);
        uint32_t t4 = w4 + hy_rotl32(w3, 11);
        uint32_t t5 = w5 + hy_rotl32(w4, 11);
        uint32_t t6 = w6 + hy_rotl32(w5, 11);
        uint32_t t7 = w7 + hy_rotl32(w6, 11);
        HY_LANES4_BULK(h4, h5, h6, h7, t4, t5, t6, t7, C);
        wp = w7;
        h0 += wp;
        p += 32;
        length -= 32;
    }
    state.h[0] = h0;
    state.h[1] = h1;
    state.h[2] = h2;
    state.h[3] = h3;
    state.h[4] = h4;
    state.h[5] = h5;
    state.h[6] = h6;
    state.h[7] = h7;
    state.previous = wp;
    state.carry = C;
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
    uint32_t wp = 0, C = 0;

    if (l >= 128) {
        hy_bulk_state bulk = {{
            h0,
            h1,
            h2,
            h3,
            s1 + HY_KE,
            hy_rotl32(s0, 9) ^ HY_KD,
            hy_rotl32(s1, 18) + HY_KA,
            hy_rotl32(s0, 27) ^ HY_KB,
        }, wp, C};
        const size_t blocks = l & ~(size_t)31;
        hy_bulk<bswap>(bulk, p, blocks);
        h0 = bulk.h[0];
        h1 = bulk.h[1];
        h2 = bulk.h[2];
        h3 = bulk.h[3];
        uint32_t h4 = bulk.h[4];
        uint32_t h5 = bulk.h[5];
        uint32_t h6 = bulk.h[6];
        uint32_t h7 = bulk.h[7];
        wp = bulk.previous;
        C = bulk.carry;
        p += blocks;
        l -= blocks;
        h0 = (uint32_t)((uint64_t)(h0 ^ hy_rotl32(h4, 11)) * HY_KA) ^ C;
        h1 = (uint32_t)((uint64_t)(h1 ^ hy_rotl32(h5, 19)) * HY_KB);
        h2 = (uint32_t)((uint64_t)(h2 ^ hy_rotl32(h6, 7)) * HY_KC) ^
             hy_rotl32(C, 16);
        h3 = (uint32_t)((uint64_t)(h3 ^ hy_rotl32(h7, 23)) * HY_KD);
    }

    while (l >= 16) {
        uint32_t w0 = GET_U32<bswap>(p, 0);
        uint32_t w1 = GET_U32<bswap>(p, 4);
        uint32_t w2 = GET_U32<bswap>(p, 8);
        uint32_t w3 = GET_U32<bswap>(p, 12);
        uint32_t t0 = w0 + hy_rotl32(wp, 11);
        uint32_t t1 = w1 + hy_rotl32(w0, 11);
        uint32_t t2 = w2 + hy_rotl32(w1, 11);
        uint32_t t3 = w3 + hy_rotl32(w2, 11);
        HY_LANES4_MID(h0, h1, h2, h3, t0, t1, t2, t3, C);
        wp = w3;
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
