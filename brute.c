/* Simulate parallel brute forcing of a hash.  We look for collisions
 * on the first few bits of MD5.  */

/* In a real parallel implementation, the calls to the function unit
 * below would be farmed out to multiple processors.  This is only a
 * simulation so we only do one call at a time...  */

/* Compile with something like
 *
 * gcc -O3 -O2 -flax-vector-conversions -msse -msse2 -march=athlon64 -mtune=athlon64 -Wall -Winline -std=gnu99 -fomit-frame-pointer brute.c -msse2 -o brute
 *
 * Your compiler better be good at constant folding inline functions
 * and static const variables - you may wanna check the assembly...
 */

#include <assert.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define inline __attribute__ ((always_inline))

static inline uint32_t SWAP (uint32_t a)
{
    return __builtin_bswap32 (a);
}


/* Bits of hash to take.  The hash function we compute collisions for
 * is the first BITS bits of the md5 value.  The program will require
 * around 1<<(BITS/2) md5 operations.  */
#define BITS 80

/* Proportion out of 1<<32 of results to take.  */
#define PROPORTION 16

/* When we say inline we mean it --- we're relying heavily on constant
 * folding functions such as word() below.  */
#define INLINE __attribute__ ((always_inline))

#define DEBUG(...) fprintf (stderr, __VA_ARGS__)
#define NODEBUG(...) while (0) fprintf (stderr, __VA_ARGS__)

/************************* MMX stuff.  **************************/

/* Number of 32 bit words in a register.  */
#define WIDTH 8

typedef unsigned __attribute__ ((vector_size (16))) value4_t;
static INLINE value4_t DIAG4 (uint32_t a)
{
    return (value4_t) { a, a, a, a };
}


static INLINE uint32_t FIRST4 (value4_t A)
{
    typedef unsigned u128 __attribute__ ((mode (TI)));
    return (u128) A;
}


static inline value4_t ANDN4 (value4_t a, value4_t b)
{
    return __builtin_ia32_pandn128 (a, b);
}


#define RIGHT4(a,n) __builtin_ia32_psrldi128 (a, n)
#define LEFT4(a,n) __builtin_ia32_pslldi128 (a, n)

static INLINE int TEST4 (value4_t v)
{
    value4_t b = __builtin_ia32_pcmpgtd128 (DIAG4 (PROPORTION + 0x80000000), v);
    return __builtin_ia32_pmovmskb128 (b);
}

static INLINE bool TESTTEST4 (int m, int index)
{
    return m & (1 << (index * 4));
}


static uint32_t EXTRACT4 (value4_t v, int index)
{
    switch (index) {
    case 0:
        return FIRST4 (v);
    case 1:
        return FIRST4 (__builtin_ia32_pshufd (v, 0x55));
    case 2:
        return FIRST4 (__builtin_ia32_pshufd (v, 0xAA));
    case 3:
        return FIRST4 (__builtin_ia32_pshufd (v, 0xFF));
    default:
        abort();
    }
}


static value4_t INSERT4 (value4_t v, int index, uint32_t vv)
{
    value4_t mask;
    value4_t ins;

    switch (index) {
    case 0:
        mask = (value4_t) { 0, -1, -1, -1 };
        ins = (value4_t) { vv, 0, 0, 0 };
        break;
    case 1:
        mask = (value4_t) { -1, 0, -1, -1 };
        ins = (value4_t) { 0, vv, 0, 0 };
        break;
    case 2:
        mask = (value4_t) { -1, -1, 0, -1 };
        ins = (value4_t) { 0, 0, vv, 0 };
        break;
    case 3:
        mask = (value4_t) { -1, -1, -1, 0 };
        ins = (value4_t) { 0, 0, 0, vv };
        break;
    default:
        abort();
    }
    return (v & mask) | ins;
}



static INLINE void BYTE_INTERLEAVE4 (value4_t * __restrict__ lo,
                                     value4_t * __restrict__ hi)
{
#define BIDEBUG NODEBUG
    BIDEBUG ("I0: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             SWAP (EXTRACT4 (*lo, 0)),
             SWAP (EXTRACT4 (*lo, 1)),
             SWAP (EXTRACT4 (*lo, 2)),
             SWAP (EXTRACT4 (*lo, 3)),
             SWAP (EXTRACT4 (*hi, 0)),
             SWAP (EXTRACT4 (*hi, 1)),
             SWAP (EXTRACT4 (*hi, 2)),
             SWAP (EXTRACT4 (*hi, 3)));

    // lo 8 bytes interleaved into 16 bytes.
    value4_t lo1 = __builtin_ia32_punpcklbw128 (*lo, *hi);
    // hi 8 bytes interleaved into 16 bytes.
    value4_t hi1 = __builtin_ia32_punpckhbw128 (*lo, *hi);

    BIDEBUG ("I1: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             SWAP (EXTRACT4 (lo1, 0)),
             SWAP (EXTRACT4 (lo1, 1)),
             SWAP (EXTRACT4 (lo1, 2)),
             SWAP (EXTRACT4 (lo1, 3)),
             SWAP (EXTRACT4 (hi1, 0)),
             SWAP (EXTRACT4 (hi1, 1)),
             SWAP (EXTRACT4 (hi1, 2)),
             SWAP (EXTRACT4 (hi1, 3)));

    // We now have the correct 32-bit words, only in the wrong positions.
    // Each 32 bit value has been expanded to 64, so we want
    // lo.0 = lo1.0, hi.0 = lo1.1,
    // lo.1 = lo1.2, hi.1 = lo1.3,
    // lo.2 = hi1.0, hi.2 = hi1.1,
    // lo.3 = hi1.2, lo.3 = hi1.3
    // If we swap the middle pair of 32 bits in each of lo1 and lo2, then we
    // only need to shuffle around 64 bit quantities:
    value4_t lo2 = __builtin_ia32_pshufd (lo1, 0xd8);
    value4_t hi2 = __builtin_ia32_pshufd (hi1, 0xd8);

    BIDEBUG ("I2: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             SWAP (EXTRACT4 (lo2, 0)),
             SWAP (EXTRACT4 (lo2, 1)),
             SWAP (EXTRACT4 (lo2, 2)),
             SWAP (EXTRACT4 (lo2, 3)),
             SWAP (EXTRACT4 (hi2, 0)),
             SWAP (EXTRACT4 (hi2, 1)),
             SWAP (EXTRACT4 (hi2, 2)),
             SWAP (EXTRACT4 (hi2, 3)));

    // Now we want
    // lo.0 = lo2.0, hi.0 = lo2.2,
    // lo.1 = lo2.1, hi.1 = lo2.3,
    // lo.2 = hi2.0, hi.2 = hi2.2,
    // lo.3 = hi2.1, lo.3 = hi2.3
    *lo = __builtin_ia32_punpcklqdq128 (lo2, hi2);
    *hi = __builtin_ia32_punpckhqdq128 (lo2, hi2);

    BIDEBUG ("I3: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             SWAP (EXTRACT4 (*lo, 0)),
             SWAP (EXTRACT4 (*lo, 1)),
             SWAP (EXTRACT4 (*lo, 2)),
             SWAP (EXTRACT4 (*lo, 3)),
             SWAP (EXTRACT4 (*hi, 0)),
             SWAP (EXTRACT4 (*hi, 1)),
             SWAP (EXTRACT4 (*hi, 2)),
             SWAP (EXTRACT4 (*hi, 3)));
}

// Convert a collection of nibbles to ASCII values.
static INLINE value4_t ASCIIFY4 (value4_t nibbles)
{
    value4_t adjust = ((value4_t) __builtin_ia32_pcmpgtb128 (
                          nibbles, DIAG4 (0x09090909)))
        & DIAG4 (0x27272727);
    value4_t result = __builtin_ia32_paddb128 (nibbles, DIAG4 (0x30303030));
    return result + adjust;
}


#if WIDTH == 4

typedef value4_t value_t;
#define DIAG DIAG4
#define FIRST FIRST4
#define ANDN ANDN4
#define RIGHT RIGHT4
#define LEFT LEFT4
#define TEST TEST4
#define TESTTEST TESTTEST4
#define EXTRACT EXTRACT4
#define INSERT INSERT4

#elif WIDTH == 8

typedef struct value_t {
    value4_t a;
    value4_t b;
} value_t;


static INLINE value_t DIAG (uint32_t v)
{
    return (value_t) { DIAG4 (v), DIAG4 (v) };
}
static INLINE uint32_t FIRST (value_t v)
{
    return FIRST4 (v.a);
}
static INLINE value_t AND (value_t x, value_t y)
{
    return (value_t) { x.a & y.a, x.b & y.b };
}
static INLINE value_t ANDN (value_t x, value_t y)
{
    return (value_t) { ANDN4 (x.a, y.a), ANDN4 (x.b, y.b) };
}
static INLINE value_t NOT (value_t x)
{
    return (value_t) { ~x.a, ~x.b };
}
static INLINE value_t OR (value_t x, value_t y)
{
    return (value_t) { x.a | y.a, x.b | y.b };
}
static INLINE value_t XOR (value_t x, value_t y)
{
    return (value_t) { x.a ^ y.a, x.b ^ y.b };
}
static INLINE value_t ADD (value_t x, value_t y)
{
    return (value_t) { x.a + y.a, x.b + y.b };
}
static INLINE value_t LEFT (value_t x, uint32_t n)
{
    return (value_t) { LEFT4 (x.a, n), LEFT4 (x.b, n) };
}
static INLINE value_t RIGHT (value_t x, uint32_t n)
{
    return (value_t) { RIGHT4 (x.a, n), RIGHT4 (x.b, n) };
}
static INLINE int TEST (value_t v)
{
    return (TEST4 (v.b) << 16) | TEST4 (v.a);
}
static value_t INSERT (value_t v, int i, uint32_t x)
{
    if (i < 4)
        return (value_t) { INSERT4 (v.a, i, x), v.b };
    else
        return (value_t) { v.a, INSERT4 (v.b, i - 4, x) };
}
static uint32_t EXTRACT (value_t v, int i)
{
    if (i < 4)
        return EXTRACT4 (v.a, i);
    else
        return EXTRACT4 (v.b, i - 4);
}
static value_t ASCIIFY (value_t v)
{
    return (value_t) { ASCIIFY4 (v.a), ASCIIFY4 (v.b) };
}
static void BYTE_INTERLEAVE (value_t * __restrict__ x,
                             value_t * __restrict__ y)
{
    BYTE_INTERLEAVE4 (&x->a, &y->a);
    BYTE_INTERLEAVE4 (&x->b, &y->b);
}

#define TESTTEST TESTTEST4


#define DIAG DIAG
#define FIRST FIRST
#define ADD ADD
#define AND AND
#define ANDN ANDN
#define LEFT LEFT
#define NOT NOT
#define OR OR
#define RIGHT RIGHT
#define XOR XOR

#else
#error Huh? Unknown width
#endif

#ifndef ANDN
#define ANDN(a,b) (~(a) & (b))
#endif
#ifndef AND
#define AND(a,b)  ((a) & (b))
#endif
#ifndef NOT
#define NOT(a)    (~(a))
#endif
#ifndef OR
#define OR(a,b)   ((a) | (b))
#endif
#ifndef XOR
#define XOR(a,b)  ((a) ^ (b))
#endif
#ifndef ADD
#define ADD(a,b)  ((a) + (b))
#endif
#ifndef LEFT
static INLINE value_t LEFT (value_t a, int count) { return a << count; }
#endif
#ifndef RIGHT
static INLINE value_t RIGHT (value_t a, int count) { return a >> count; }
#endif
#ifndef ROTATE_LEFT
static INLINE value_t ROTATE_LEFT (value_t a, unsigned count)
{
    return OR (LEFT (a, count), RIGHT (a, 32 - count));
}
#endif
#ifndef EXPAND
static INLINE void EXPAND (value_t v,
                           value_t * __restrict__ V0,
                           value_t * __restrict__ V1)
{
    *V0 = AND (RIGHT(v, 4), DIAG (0x0f0f0f0f));
    *V1 = AND (v, DIAG (0x0f0f0f0f));
    BYTE_INTERLEAVE (V0, V1);
    *V0 = ASCIIFY (*V0);
    *V1 = ASCIIFY (*V1);

    NODEBUG ("Expand: %08x %08x %08x %08x\n"
             "        %08x %08x %08x %08x\n"
             "        %08x %08x %08x %08x\n",
             SWAP (EXTRACT (v, 0)),
             SWAP (EXTRACT (v, 1)),
             SWAP (EXTRACT (v, 2)),
             SWAP (EXTRACT (v, 3)),

             SWAP (EXTRACT (*V0, 0)),
             SWAP (EXTRACT (*V0, 1)),
             SWAP (EXTRACT (*V0, 2)),
             SWAP (EXTRACT (*V0, 3)),

             SWAP (EXTRACT (*V1, 0)),
             SWAP (EXTRACT (*V1, 1)),
             SWAP (EXTRACT (*V1, 2)),
             SWAP (EXTRACT (*V1, 3)));
}
#endif
#ifndef SPLIT
#define SPLIT(v,V0,V1) value_t V0; value_t V1; EXPAND (v, &V0, &V1);
#endif


static INLINE value_t TRIM (value_t a, unsigned index)
{
#define TRDEBUG NODEBUG
    index *= 32;
    if (index + 32 <= BITS) {
        TRDEBUG ("%u - all\n", index);
        return a;
    }
    if (index >= BITS) {
        TRDEBUG ("%u - none\n", index);
        return DIAG (0);
    }
    TRDEBUG ("%u...", index);
    index = BITS - index;
    TRDEBUG ("%u...", index);
    unsigned mask = 0xffffffff << (32 - index);
    TRDEBUG ("%08x\n", mask);

    return AND (a, DIAG (SWAP (mask)));
}

/* Bits to shift left at various stages.  */

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

/* F, G, H and I are basic MD5 functions.  */
#define F(x, y, z) OR (AND ((x), (y)), ANDN ((x), (z)))
#define G(x, y, z) OR (AND ((z), (x)), ANDN ((z), (y)))
#define H(x, y, z) XOR ((x), XOR ((y), (z)))
#define I(x, y, z) XOR ((y), OR ((x), NOT ((z))))

#define A4(a, b, c, d) ADD (ADD ((a), (b)), ADD ((c), (d)))

/* ROTATE_LEFT rotates x left n bits.  */
//#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

#define GENERATE(FUN,a, b, c, d, x, s, ac) {		\
	(a) = A4 ((a), FUN ((b), (c), (d)), (x), DIAG (ac));	\
	(a) = ROTATE_LEFT ((a), (s));			\
        (a) = ADD ((a), (b));				\
    }

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
 * Rotation is separate from addition to prevent recomputation.  */
#define FF(a, b, c, d, x, s, ac) GENERATE (F, a, b, c, d, x, s, ac)
#define GG(a, b, c, d, x, s, ac) GENERATE (G, a, b, c, d, x, s, ac)
#define HH(a, b, c, d, x, s, ac) GENERATE (H, a, b, c, d, x, s, ac)
#define II(a, b, c, d, x, s, ac) GENERATE (I, a, b, c, d, x, s, ac)

/* Generate md5 hash, storing the first 'whole' complete bytes and
 * masking the next byte by 'mask'.  Gets specialised to what we want.  */
static void INLINE inline_MD5 (value_t * __restrict D0,
                               value_t * __restrict D1,
                               value_t * __restrict D2)
{
#define MD5DEBUG NODEBUG
    MD5DEBUG ("Input is %08x %08x %08x\n",
              SWAP (FIRST (*D0)), SWAP (FIRST (*D1)), SWAP (FIRST (*D2)));

    const value_t init0 = DIAG (0x67452301);
    const value_t init1 = DIAG (0xefcdab89);
    const value_t init2 = DIAG (0x98badcfe);
    const value_t init3 = DIAG (0x10325476);

    value_t a = init0;
    value_t b = init1;
    value_t c = init2;
    value_t d = init3;

    /* Round 1 */
    SPLIT (*D0, xx00, xx01);
    FF (a, b, c, d, xx00, S11, 0xd76aa478); /* 1 */
    FF (d, a, b, c, xx01, S12, 0xe8c7b756); /* 2 */

    SPLIT (*D1, xx02, xx03);
    FF (c, d, a, b, xx02, S13, 0x242070db); /* 3 */
    FF (b, c, d, a, xx03, S14, 0xc1bdceee); /* 4 */

    SPLIT (*D2, xx04, xx05);
    FF (a, b, c, d, xx04, S11, 0xf57c0faf); /* 5 */
    FF (d, a, b, c, xx05, S12, 0x4787c62a); /* 6 */

    const value_t xx06 = DIAG (0x80);
    FF (c, d, a, b, xx06, S13, 0xa8304613); /* 7 */

    const value_t xx07 = DIAG (0);
    FF (b, c, d, a, xx07, S14, 0xfd469501); /* 8 */

    const value_t xx08 = DIAG (0);
    FF (a, b, c, d, xx08, S11, 0x698098d8); /* 9 */

    const value_t xx09 = DIAG (0);
    FF (d, a, b, c, xx09, S12, 0x8b44f7af); /* 10 */

    const value_t xx10 = DIAG (0);
    FF (c, d, a, b, xx10, S13, 0xffff5bb1); /* 11 */

    const value_t xx11 = DIAG (0);
    FF (b, c, d, a, xx11, S14, 0x895cd7be); /* 12 */

    const value_t xx12 = DIAG (0);
    FF (a, b, c, d, xx12, S11, 0x6b901122); /* 13 */

    const value_t xx13 = DIAG (0);
    FF (d, a, b, c, xx13, S12, 0xfd987193); /* 14 */

    const value_t xx14 = DIAG (3 * 32 * 2);
    FF (c, d, a, b, xx14, S13, 0xa679438e); /* 15 */

    const value_t xx15 = DIAG (0);
    FF (b, c, d, a, xx15, S14, 0x49b40821); /* 16 */

    MD5DEBUG ("BLOCK IS is \n"
              "  %08x %08x %08x %08x %08x %08x %08x %08x\n"
              "  %08x %08x %08x %08x %08x %08x %08x %08x\n",
              FIRST (xx00), FIRST (xx01), FIRST (xx02), FIRST (xx03),
              FIRST (xx04), FIRST (xx05), FIRST (xx06), FIRST (xx07),
              FIRST (xx08), FIRST (xx09), FIRST (xx10), FIRST (xx11),
              FIRST (xx12), FIRST (xx13), FIRST (xx14), FIRST (xx15));

    /* Round 2 */
    GG (a, b, c, d, xx01, S21, 0xf61e2562); /* 17 */
    GG (d, a, b, c, xx06, S22, 0xc040b340); /* 18 */
    GG (c, d, a, b, xx11, S23, 0x265e5a51); /* 19 */
    GG (b, c, d, a, xx00, S24, 0xe9b6c7aa); /* 20 */
    GG (a, b, c, d, xx05, S21, 0xd62f105d); /* 21 */
    GG (d, a, b, c, xx10, S22,  0x2441453); /* 22 */
    GG (c, d, a, b, xx15, S23, 0xd8a1e681); /* 23 */
    GG (b, c, d, a, xx04, S24, 0xe7d3fbc8); /* 24 */
    GG (a, b, c, d, xx09, S21, 0x21e1cde6); /* 25 */
    GG (d, a, b, c, xx14, S22, 0xc33707d6); /* 26 */
    GG (c, d, a, b, xx03, S23, 0xf4d50d87); /* 27 */
    GG (b, c, d, a, xx08, S24, 0x455a14ed); /* 28 */
    GG (a, b, c, d, xx13, S21, 0xa9e3e905); /* 29 */
    GG (d, a, b, c, xx02, S22, 0xfcefa3f8); /* 30 */
    GG (c, d, a, b, xx07, S23, 0x676f02d9); /* 31 */
    GG (b, c, d, a, xx12, S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH (a, b, c, d, xx05, S31, 0xfffa3942); /* 33 */
    HH (d, a, b, c, xx08, S32, 0x8771f681); /* 34 */
    HH (c, d, a, b, xx11, S33, 0x6d9d6122); /* 35 */
    HH (b, c, d, a, xx14, S34, 0xfde5380c); /* 36 */
    HH (a, b, c, d, xx01, S31, 0xa4beea44); /* 37 */
    HH (d, a, b, c, xx04, S32, 0x4bdecfa9); /* 38 */
    HH (c, d, a, b, xx07, S33, 0xf6bb4b60); /* 39 */
    HH (b, c, d, a, xx10, S34, 0xbebfbc70); /* 40 */
    HH (a, b, c, d, xx13, S31, 0x289b7ec6); /* 41 */
    HH (d, a, b, c, xx00, S32, 0xeaa127fa); /* 42 */
    HH (c, d, a, b, xx03, S33, 0xd4ef3085); /* 43 */
    HH (b, c, d, a, xx06, S34,  0x4881d05); /* 44 */
    HH (a, b, c, d, xx09, S31, 0xd9d4d039); /* 45 */
    HH (d, a, b, c, xx12, S32, 0xe6db99e5); /* 46 */
    HH (c, d, a, b, xx15, S33, 0x1fa27cf8); /* 47 */
    HH (b, c, d, a, xx02, S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II (a, b, c, d, xx00, S41, 0xf4292244); /* 49 */
    II (d, a, b, c, xx07, S42, 0x432aff97); /* 50 */
    II (c, d, a, b, xx14, S43, 0xab9423a7); /* 51 */
    II (b, c, d, a, xx05, S44, 0xfc93a039); /* 52 */
    II (a, b, c, d, xx12, S41, 0x655b59c3); /* 53 */
    II (d, a, b, c, xx03, S42, 0x8f0ccc92); /* 54 */
    II (c, d, a, b, xx10, S43, 0xffeff47d); /* 55 */
    II (b, c, d, a, xx01, S44, 0x85845dd1); /* 56 */
    II (a, b, c, d, xx08, S41, 0x6fa87e4f); /* 57 */
    II (d, a, b, c, xx15, S42, 0xfe2ce6e0); /* 58 */
    II (c, d, a, b, xx06, S43, 0xa3014314); /* 59 */
    II (b, c, d, a, xx13, S44, 0x4e0811a1); /* 60 */
    II (a, b, c, d, xx04, S41, 0xf7537e82); /* 61 */
    II (d, a, b, c, xx11, S42, 0xbd3af235); /* 62 */
    II (c, d, a, b, xx02, S43, 0x2ad7d2bb); /* 63 */
    II (b, c, d, a, xx09, S44, 0xeb86d391); /* 64 */

    a = ADD (a, init0);
    b = ADD (b, init1);
    c = ADD (c, init2);
    d = ADD (d, init3);

    *D0 = TRIM (a, 0);
    *D1 = TRIM (b, 1);
    *D2 = TRIM (c, 2);

    MD5DEBUG ("Result is %08x %08x %08x\n",
              SWAP (FIRST (*D0)), SWAP (FIRST (*D1)), SWAP (FIRST (*D2)));
}


static void outline_MD5 (value_t * v)
{
    inline_MD5 (v, v + 1, v + 2);
}


static int loop_MD5 (value_t * __restrict__ v,
                     uint64_t * __restrict__ iterations)
{
    value_t A = v[0];
    value_t B = v[1];
    value_t C = v[2];
    int t;

    do {
        inline_MD5 (&A, &B, &C);
        ++*iterations;
        t = TEST (A);
    }
    while (__builtin_expect (t == 0, 1));
    v[0] = A;
    v[1] = B;
    v[2] = C;
    return t;
}


static uint64_t work_block = 1;
static uint64_t work_step = 1;

// (Re)initialise slice given by index.
static void recharge (value_t * v,
                      uint64_t * blocks,
                      uint64_t * starts,
                      uint64_t iteration,
                      int index)
{
    uint64_t block = work_block;
    work_block += work_step;
    v[0] = INSERT (v[0], index, block);
    v[1] = INSERT (v[1], index, block >> 32);
    v[2] = INSERT (v[2], index, 0);

    blocks[index] = block;
    starts[index] = iteration;
}


typedef struct record_t {
    uint32_t h[3];
    uint64_t block;
    uint64_t iterations;
    struct record_t * next;
} record_t;

static uint64_t recorded_iterations;

#define TABLE_SIZE 4095
static record_t * record_table[TABLE_SIZE];

static void collision (const record_t * A,
                       const record_t * B)
{
    printf ("Collide after %lu recorded iterations:\n"
            " %08x %08x ->%11lu -> %08x %08x %08x\n"
            " %08x %08x ->%11lu -> %08x %08x %08x\n",
            recorded_iterations,
            SWAP (A->block), SWAP (A->block >> 32), A->iterations,
            SWAP (A->h[0]),  SWAP (A->h[1]), SWAP (A->h[2]),
            SWAP (B->block), SWAP (B->block >> 32), B->iterations,
            SWAP (B->h[0]),  SWAP (B->h[1]), SWAP (B->h[2]));
    fflush (NULL);

    if (A->iterations < B->iterations) {
        const record_t * C = A;
        A = B;
        B = C;
    }

    value_t V[3];
    V[0] = DIAG (A->block);
    V[1] = DIAG (A->block >> 32);
    V[2] = DIAG (0);

    if (A->iterations > B->iterations) {
        value_t P[3];
        for (uint64_t i = B->iterations; i != A->iterations; ++i) {
            P[0] = V[0];
            P[1] = V[1];
            P[2] = V[2];
            outline_MD5 (V);
        }
        if (FIRST (V[0]) == (B->block & 0xffffffff) &&
            FIRST (V[1]) == (B->block >> 32) &&
            FIRST (V[2]) == 0) {
            printf (
                "Preimage: %08x %08x %08x -> %08x %08x %08x\n",
                SWAP (FIRST (P[0])), SWAP (FIRST (P[1])), SWAP (FIRST (P[2])),
                SWAP (FIRST (V[0])), SWAP (FIRST (V[1])), SWAP (FIRST (V[2])));
            fflush (NULL);
            return;
        }
    }

    V[0] = INSERT (V[0], 1, B->block);
    V[1] = INSERT (V[1], 1, B->block >> 32);
    V[2] = INSERT (V[2], 1, 0);

    assert (EXTRACT (V[0], 0) != EXTRACT (V[0], 1) ||
            EXTRACT (V[1], 0) != EXTRACT (V[1], 1) ||
            EXTRACT (V[2], 0) != EXTRACT (V[2], 1));

    for (uint64_t i = 0; i != B->iterations; ++i) {
        value_t P[3];
        P[0] = V[0];
        P[1] = V[1];
        P[2] = V[2];
        outline_MD5 (V);
        if (EXTRACT (V[0], 0) == EXTRACT (V[0], 1) &&
            EXTRACT (V[1], 0) == EXTRACT (V[1], 1) &&
            EXTRACT (V[2], 0) == EXTRACT (V[2], 1)) {
            printf ("Collide:\n"
                    "  %08x %08x %08x -> %08x %08x %08x\n"
                    "  %08x %08x %08x -> %08x %08x %08x\n",
                    SWAP (EXTRACT (P[0], 0)),
                    SWAP (EXTRACT (P[1], 0)),
                    SWAP (EXTRACT (P[2], 0)),
                    SWAP (EXTRACT (V[0], 0)),
                    SWAP (EXTRACT (V[1], 0)),
                    SWAP (EXTRACT (V[2], 0)),
                    SWAP (EXTRACT (P[0], 1)),
                    SWAP (EXTRACT (P[1], 1)),
                    SWAP (EXTRACT (P[2], 1)),
                    SWAP (EXTRACT (V[0], 1)),
                    SWAP (EXTRACT (V[1], 1)),
                    SWAP (EXTRACT (V[2], 1)));
            fflush (NULL);
            exit (EXIT_SUCCESS);
        }
    }

    printf ("Huh?  %0x8 %08x %08x   %08x %08x %08x\n",
            SWAP (EXTRACT (V[0], 0)),
            SWAP (EXTRACT (V[1], 0)),
            SWAP (EXTRACT (V[2], 0)),
            SWAP (EXTRACT (V[0], 1)),
            SWAP (EXTRACT (V[1], 1)),
            SWAP (EXTRACT (V[2], 1)));
    fflush (NULL);
    abort();
}

static __thread struct timeval last_time;
static __thread uint64_t last_iters;

static void store (value_t * v,
                   uint64_t * blocks,
                   uint64_t * starts,
                   uint64_t iteration,
                   int index)
{
    struct timeval current_time;
    gettimeofday (&current_time, NULL);
    uint64_t milliseconds = current_time.tv_sec - last_time.tv_sec;
    milliseconds *= 1000000;
    milliseconds += current_time.tv_usec - last_time.tv_usec;
    milliseconds /= 1000;

    record_t * record = malloc (sizeof (record_t));
    record->h[0] = EXTRACT (v[0], index);
    record->h[1] = EXTRACT (v[1], index);
    record->h[2] = EXTRACT (v[2], index);
    record->block = blocks[index];
    record->iterations = iteration - starts[index];
    recorded_iterations += record->iterations;

    printf ("%08x %08x -%10lu - %08x %08x %08x [%lu,%lu]\n",
            SWAP (record->block), SWAP (record->block >> 32),
            record->iterations,
            SWAP (record->h[0]), SWAP (record->h[1]), SWAP (record->h[2]),
            recorded_iterations,
            (iteration - last_iters) * 1000 / milliseconds);
    fflush (NULL);
    last_time = current_time;
    last_iters = iteration;

    uint32_t hash = record->h[0] ^ record->h[1] ^ record->h[2];
    hash %= TABLE_SIZE;

    for (const record_t * p = record_table[hash]; p; p = p->next)
        if (p->h[1] == record->h[1] &&
            p->h[0] == record->h[0] &&
            p->h[2] == record->h[2])
            collision (p, record);

    record->next = record_table[hash];
    record_table[hash] = record;
}

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void main_loop (void * ignore) __attribute__ ((__noreturn__));
static void main_loop (void * ignore)
{
    gettimeofday (&last_time, NULL);

    value_t V[3];
    V[0] = V[1] = V[2] = DIAG (0);
    uint64_t blocks[WIDTH];
    uint64_t starts[WIDTH];
    uint64_t iteration = 0;

    pthread_mutex_lock (&lock);
    for (int i = 0; i != WIDTH; ++i)
        recharge (V, blocks, starts, iteration, i);
    pthread_mutex_unlock (&lock);

    while (true) {
        int t = loop_MD5 (V, &iteration);

        for (int i = 0; i != WIDTH; ++i)
            if (TESTTEST (t, i)) {
                pthread_mutex_lock (&lock);
                store (V, blocks, starts, iteration, i);
                recharge (V, blocks, starts, iteration, i);
                pthread_mutex_unlock (&lock);
            }
    }
}


int main (int argc, char ** argv)
{
    if (argc > 1)
        work_block = strtoul (argv[1], NULL, 0);
    if (argc > 2)
        work_step = strtoul (argv[2], NULL, 0);

    value_t v = DIAG (0);


    uint32_t v0 = 0xdeadbeaf;
    uint32_t v1 = 0xcae3159a;
    uint32_t v2 = 0x12345678;
    uint32_t v3 = 0xabcdef09;

    v = INSERT (v, 0, v0);
    v = INSERT (v, 1, v1);
    v = INSERT (v, 2, v2);
    v = INSERT (v, 3, v3);

    assert (v0 == EXTRACT (v, 0));
    assert (v1 == EXTRACT (v, 1));
    assert (v2 == EXTRACT (v, 2));
    assert (v3 == EXTRACT (v, 3));

/*     pthread_t th; */
/*     pthread_create (&th, NULL, main_loop, NULL); */
    main_loop (NULL);
}
