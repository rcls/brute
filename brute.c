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

static inline uint32_t SWAP (uint32_t a)
{
    return __builtin_bswap32 (a);
}


/* Bits of hash to take.  The hash function we compute collisions for
 * is the first BITS bits of the md5 value.  The program will require
 * around 1<<(BITS/2) md5 operations.  */
#define BITS 55

/* Proportion out of 1<<32 of results to take.  */
#define PROPORTION 512

/* When we say inline we mean it --- we're relying heavily on constant
 * folding functions such as word() below.  */
#define INLINE __attribute__ ((always_inline))

#define DEBUG(...) fprintf (stderr, __VA_ARGS__)
#define NODEBUG(...) while (0) fprintf (stderr, __VA_ARGS__)

/************************* MMX stuff.  **************************/

/* Number of 32 bit words in a register.  */
#define WIDTH 4

#if WIDTH == 4

typedef unsigned __attribute__ ((vector_size (4 * WIDTH))) value_t;
static INLINE value_t DIAG (uint32_t a)
{
    return (value_t) { a, a, a, a };
}


static INLINE uint32_t FIRST (value_t A)
{
    typedef int int128 __attribute__ ((mode (TI)));
    return (int128) A;
}


static inline value_t ANDN (value_t a, value_t b)
{
    return __builtin_ia32_pandn128 (a, b);
}
#define ANDN ANDN


#define GREATER_THAN(a, b) __builtin_ia32_pcmpgtd128 (a, b)

#define RIGHT(a,n) __builtin_ia32_psrldi128 (a, n)
#define LEFT(a,n) __builtin_ia32_pslldi128 (a, n)


static INLINE int TEST (value_t v)
{
    value_t b = __builtin_ia32_pcmpgtd128 (DIAG (PROPORTION + 0x80000000), v);
    return __builtin_ia32_pmovmskb128 (b);
}

static INLINE bool TESTTEST (int m, int index)
{
    return m & (1 << (index * 4));
}


static INLINE uint32_t EXTRACTi (value_t v, int index)
{
    switch (index) {
    case 0:
        return FIRST (v);
    case 1:
        return FIRST (__builtin_ia32_pshufd (v, 0x55));
    case 2:
        return FIRST (__builtin_ia32_pshufd (v, 0xAA));
    case 3:
        return FIRST (__builtin_ia32_pshufd (v, 0xFF));
    default:
        abort();
    }
}


static INLINE value_t INSERTi (value_t v, int index, uint32_t vv)
{
    value_t mask;
    value_t ins;

    switch (index) {
    case 0:
        mask = (value_t) { 0, -1, -1, -1 };
        ins = (value_t) { vv, 0, 0, 0 };
        break;
    case 1:
        mask = (value_t) { -1, 0, -1, -1 };
        ins = (value_t) { 0, vv, 0, 0 };
        break;
    case 2:
        mask = (value_t) { -1, -1, 0, -1 };
        ins = (value_t) { 0, 0, vv, 0 };
        break;
    case 3:
        mask = (value_t) { -1, -1, -1, 0 };
        ins = (value_t) { 0, 0, 0, vv };
        break;
    default:
        abort();
    }
    return (v & mask) | ins;
}



static INLINE void BYTE_INTERLEAVE (value_t * lo, value_t * hi)
{
#define BIDEBUG NODEBUG
    BIDEBUG ("I0: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             SWAP (EXTRACTi (*lo, 0)),
             SWAP (EXTRACTi (*lo, 1)),
             SWAP (EXTRACTi (*lo, 2)),
             SWAP (EXTRACTi (*lo, 3)),
             SWAP (EXTRACTi (*hi, 0)),
             SWAP (EXTRACTi (*hi, 1)),
             SWAP (EXTRACTi (*hi, 2)),
             SWAP (EXTRACTi (*hi, 3)));

    // lo 8 bytes interleaved into 16 bytes.
    value_t lo1 = __builtin_ia32_punpcklbw128 (*lo, *hi);
    // hi 8 bytes interleaved into 16 bytes.
    value_t hi1 = __builtin_ia32_punpckhbw128 (*lo, *hi);

    BIDEBUG ("I1: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             SWAP (EXTRACTi (lo1, 0)),
             SWAP (EXTRACTi (lo1, 1)),
             SWAP (EXTRACTi (lo1, 2)),
             SWAP (EXTRACTi (lo1, 3)),
             SWAP (EXTRACTi (hi1, 0)),
             SWAP (EXTRACTi (hi1, 1)),
             SWAP (EXTRACTi (hi1, 2)),
             SWAP (EXTRACTi (hi1, 3)));

    // We now have the correct 32-bit words, only in the wrong positions.
    // Each 32 bit value has been expanded to 64, so we want
    // lo.0 = lo1.0, hi.0 = lo1.1,
    // lo.1 = lo1.2, hi.1 = lo1.3,
    // lo.2 = hi1.0, hi.2 = hi1.1,
    // lo.3 = hi1.2, lo.3 = hi1.3
    // If we swap the middle pair of 32 bits in each of lo1 and lo2, then we
    // only need to shuffle around 64 bit quantities:
    value_t lo2 = __builtin_ia32_pshufd (lo1, 0xd8);
    value_t hi2 = __builtin_ia32_pshufd (hi1, 0xd8);

    BIDEBUG ("I2: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             SWAP (EXTRACTi (lo2, 0)),
             SWAP (EXTRACTi (lo2, 1)),
             SWAP (EXTRACTi (lo2, 2)),
             SWAP (EXTRACTi (lo2, 3)),
             SWAP (EXTRACTi (hi2, 0)),
             SWAP (EXTRACTi (hi2, 1)),
             SWAP (EXTRACTi (hi2, 2)),
             SWAP (EXTRACTi (hi2, 3)));

    // Now we want
    // lo.0 = lo2.0, hi.0 = lo2.2,
    // lo.1 = lo2.1, hi.1 = lo2.3,
    // lo.2 = hi2.0, hi.2 = hi2.2,
    // lo.3 = hi2.1, lo.3 = hi2.3
    *lo = __builtin_ia32_punpcklqdq128 (lo2, hi2);
    *hi = __builtin_ia32_punpckhqdq128 (lo2, hi2);

    BIDEBUG ("I3: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             SWAP (EXTRACTi (*lo, 0)),
             SWAP (EXTRACTi (*lo, 1)),
             SWAP (EXTRACTi (*lo, 2)),
             SWAP (EXTRACTi (*lo, 3)),
             SWAP (EXTRACTi (*hi, 0)),
             SWAP (EXTRACTi (*hi, 1)),
             SWAP (EXTRACTi (*hi, 2)),
             SWAP (EXTRACTi (*hi, 3)));
}

// Convert a collection of nibbles to ASCII values.
static INLINE value_t ASCIIFY (value_t nibbles)
{
    value_t adjust = ((value_t) __builtin_ia32_pcmpgtb128 (
                          nibbles, DIAG (0x09090909)))
        & DIAG (0x27272727);
    value_t result = __builtin_ia32_paddb128 (nibbles, DIAG (0x30303030));
    return result + adjust;
}

static INLINE value_t LOW_NIBBLE (value_t w)
{
    return w & DIAG (0x0f0f0f0f);
}

static INLINE value_t HIGH_NIBBLE (value_t w)
{
    return RIGHT(w, 4) & DIAG (0x0f0f0f0f);
}

static INLINE void EXPAND (value_t v, value_t * V0, value_t * V1)
{
    *V0 = HIGH_NIBBLE (v);
    *V1 = LOW_NIBBLE (v);
    BYTE_INTERLEAVE (V0, V1);
    *V0 = ASCIIFY (*V0);
    *V1 = ASCIIFY (*V1);

    NODEBUG ("Expand: %08x %08x %08x %08x\n"
             "        %08x %08x %08x %08x\n"
             "        %08x %08x %08x %08x\n",
             SWAP (EXTRACTi (v, 0)),
             SWAP (EXTRACTi (v, 1)),
             SWAP (EXTRACTi (v, 2)),
             SWAP (EXTRACTi (v, 3)),

             SWAP (EXTRACTi (*V0, 0)),
             SWAP (EXTRACTi (*V0, 1)),
             SWAP (EXTRACTi (*V0, 2)),
             SWAP (EXTRACTi (*V0, 3)),

             SWAP (EXTRACTi (*V1, 0)),
             SWAP (EXTRACTi (*V1, 1)),
             SWAP (EXTRACTi (*V1, 2)),
             SWAP (EXTRACTi (*V1, 3)));
}


#else

#error Huh

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
#ifndef SUB
#define SUB(a,b)  ((a) - (b))
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


static uint32_t EXTRACTo (value_t v, int index)
{
    return EXTRACTi (v, index);
}

static INLINE uint32_t EXTRACT (value_t v, int index)
{
    if (__builtin_constant_p (index))
        return EXTRACTi (v, index);
    else
        return EXTRACTo (v, index);
}


static value_t INSERTo (value_t v, int index, uint32_t vv)
{
    return INSERTi (v, index, vv);
}

static INLINE value_t INSERT (value_t v, int index, uint32_t vv)
{
    if (__builtin_constant_p (index))
        return INSERTi (v, index, vv);
    else
        return INSERTo (v, index, vv);
}

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

#define GENERATE1(FUN,a, b, c, d, x, s, ac) {		\
	(a) = A4 ((a), FUN ((b), (c), (d)), (x), DIAG (ac));	\
	(a) = ROTATE_LEFT ((a), (s));			\
        (a) = ADD ((a), (b));				\
    }

#define GENERATE(FUN,a, b, c, d, x, s, ac) {                            \
	X##a = A4 (X##a, FUN (X##b, X##c, X##d), X##x, DIAG (ac));      \
	Y##a = A4 (Y##a, FUN (Y##b, Y##c, Y##d), Y##x, DIAG (ac));      \
	X##a = ROTATE_LEFT (X##a, s);                                   \
	Y##a = ROTATE_LEFT (Y##a, s);                                   \
        X##a = ADD (X##a, X##b);                                        \
        Y##a = ADD (Y##a, Y##b);                                        \
    }

#define SPLIT(v,V0,V1)                                     \
    value_t X##V0; value_t X##V1; EXPAND (*X##v, &X##V0, &X##V1); \
    value_t Y##V0; value_t Y##V1; EXPAND (*Y##v, &Y##V0, &Y##V1);


/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
 * Rotation is separate from addition to prevent recomputation.  */
#define FF(a, b, c, d, x, s, ac) GENERATE (F, a, b, c, d, x, s, ac)
#define GG(a, b, c, d, x, s, ac) GENERATE (G, a, b, c, d, x, s, ac)
#define HH(a, b, c, d, x, s, ac) GENERATE (H, a, b, c, d, x, s, ac)
#define II(a, b, c, d, x, s, ac) GENERATE (I, a, b, c, d, x, s, ac)

/* Generate md5 hash, storing the first 'whole' complete bytes and
 * masking the next byte by 'mask'.  Gets specialised to what we want.  */
static void INLINE inline_MD5 (value_t * __restrict Xv0,
                               value_t * __restrict Xv1,
                               value_t * __restrict Xv2,
                               value_t * __restrict Yv0,
                               value_t * __restrict Yv1,
                               value_t * __restrict Yv2)
{
#define MD5DEBUG NODEBUG
    MD5DEBUG ("Input is %08x %08x %08x\n",
              SWAP (FIRST (*Xv0)), SWAP (FIRST (*Xv1)), SWAP (FIRST (*Xv2)));

    const value_t init0 = DIAG (0x67452301);
    const value_t init1 = DIAG (0xefcdab89);
    const value_t init2 = DIAG (0x98badcfe);
    const value_t init3 = DIAG (0x10325476);

    value_t Xa = init0;
    value_t Xb = init1;
    value_t Xc = init2;
    value_t Xd = init3;
    value_t Ya = init0;
    value_t Yb = init1;
    value_t Yc = init2;
    value_t Yd = init3;

    /* Round 1 */
    SPLIT (v0, t00, t01);
    FF (a, b, c, d, t00, S11, 0xd76aa478); /* 1 */
    FF (d, a, b, c, t01, S12, 0xe8c7b756); /* 2 */

    SPLIT (v1, t02, t03);
    FF (c, d, a, b, t02, S13, 0x242070db); /* 3 */
    FF (b, c, d, a, t03, S14, 0xc1bdceee); /* 4 */

    SPLIT (v2, t04, t05);
    FF (a, b, c, d, t04, S11, 0xf57c0faf); /* 5 */
    FF (d, a, b, c, t05, S12, 0x4787c62a); /* 6 */

    const value_t Xt06 = DIAG (0x80);
    const value_t Yt06 = DIAG (0x80);
    FF (c, d, a, b, t06, S13, 0xa8304613); /* 7 */

    const value_t Xt07 = DIAG (0);
    const value_t Yt07 = DIAG (0);
    FF (b, c, d, a, t07, S14, 0xfd469501); /* 8 */

    const value_t Xt08 = DIAG (0);
    const value_t Yt08 = DIAG (0);
    FF (a, b, c, d, t08, S11, 0x698098d8); /* 9 */

    const value_t Xt09 = DIAG (0);
    const value_t Yt09 = DIAG (0);
    FF (d, a, b, c, t09, S12, 0x8b44f7af); /* 10 */

    const value_t Xt10 = DIAG (0);
    const value_t Yt10 = DIAG (0);
    FF (c, d, a, b, t10, S13, 0xffff5bb1); /* 11 */

    const value_t Xt11 = DIAG (0);
    const value_t Yt11 = DIAG (0);
    FF (b, c, d, a, t11, S14, 0x895cd7be); /* 12 */

    const value_t Xt12 = DIAG (0);
    const value_t Yt12 = DIAG (0);
    FF (a, b, c, d, t12, S11, 0x6b901122); /* 13 */

    const value_t Xt13 = DIAG (0);
    const value_t Yt13 = DIAG (0);
    FF (d, a, b, c, t13, S12, 0xfd987193); /* 14 */

    const value_t Xt14 = DIAG (3 * 32 * 2);
    const value_t Yt14 = DIAG (3 * 32 * 2);
    FF (c, d, a, b, t14, S13, 0xa679438e); /* 15 */

    const value_t Xt15 = DIAG (0);
    const value_t Yt15 = DIAG (0);
    FF (b, c, d, a, t15, S14, 0x49b40821); /* 16 */

    MD5DEBUG ("BLOCK IS is \n"
              "  %08x %08x %08x %08x %08x %08x %08x %08x\n"
              "  %08x %08x %08x %08x %08x %08x %08x %08x\n",
              FIRST (Xt00), FIRST (Xt01), FIRST (Xt02), FIRST (Xt03),
              FIRST (Xt04), FIRST (Xt05), FIRST (Xt06), FIRST (Xt07),
              FIRST (Xt08), FIRST (Xt09), FIRST (Xt10), FIRST (Xt11),
              FIRST (Xt12), FIRST (Xt13), FIRST (Xt14), FIRST (Xt15));

    /* Round 2 */
    GG (a, b, c, d, t01, S21, 0xf61e2562); /* 17 */
    GG (d, a, b, c, t06, S22, 0xc040b340); /* 18 */
    GG (c, d, a, b, t11, S23, 0x265e5a51); /* 19 */
    GG (b, c, d, a, t00, S24, 0xe9b6c7aa); /* 20 */
    GG (a, b, c, d, t05, S21, 0xd62f105d); /* 21 */
    GG (d, a, b, c, t10, S22,  0x2441453); /* 22 */
    GG (c, d, a, b, t15, S23, 0xd8a1e681); /* 23 */
    GG (b, c, d, a, t04, S24, 0xe7d3fbc8); /* 24 */
    GG (a, b, c, d, t09, S21, 0x21e1cde6); /* 25 */
    GG (d, a, b, c, t14, S22, 0xc33707d6); /* 26 */
    GG (c, d, a, b, t03, S23, 0xf4d50d87); /* 27 */
    GG (b, c, d, a, t08, S24, 0x455a14ed); /* 28 */
    GG (a, b, c, d, t13, S21, 0xa9e3e905); /* 29 */
    GG (d, a, b, c, t02, S22, 0xfcefa3f8); /* 30 */
    GG (c, d, a, b, t07, S23, 0x676f02d9); /* 31 */
    GG (b, c, d, a, t12, S24, 0x8d2a4c8a); /* 32 */

    /* Round 3 */
    HH (a, b, c, d, t05, S31, 0xfffa3942); /* 33 */
    HH (d, a, b, c, t08, S32, 0x8771f681); /* 34 */
    HH (c, d, a, b, t11, S33, 0x6d9d6122); /* 35 */
    HH (b, c, d, a, t14, S34, 0xfde5380c); /* 36 */
    HH (a, b, c, d, t01, S31, 0xa4beea44); /* 37 */
    HH (d, a, b, c, t04, S32, 0x4bdecfa9); /* 38 */
    HH (c, d, a, b, t07, S33, 0xf6bb4b60); /* 39 */
    HH (b, c, d, a, t10, S34, 0xbebfbc70); /* 40 */
    HH (a, b, c, d, t13, S31, 0x289b7ec6); /* 41 */
    HH (d, a, b, c, t00, S32, 0xeaa127fa); /* 42 */
    HH (c, d, a, b, t03, S33, 0xd4ef3085); /* 43 */
    HH (b, c, d, a, t06, S34,  0x4881d05); /* 44 */
    HH (a, b, c, d, t09, S31, 0xd9d4d039); /* 45 */
    HH (d, a, b, c, t12, S32, 0xe6db99e5); /* 46 */
    HH (c, d, a, b, t15, S33, 0x1fa27cf8); /* 47 */
    HH (b, c, d, a, t02, S34, 0xc4ac5665); /* 48 */

    /* Round 4 */
    II (a, b, c, d, t00, S41, 0xf4292244); /* 49 */
    II (d, a, b, c, t07, S42, 0x432aff97); /* 50 */
    II (c, d, a, b, t14, S43, 0xab9423a7); /* 51 */
    II (b, c, d, a, t05, S44, 0xfc93a039); /* 52 */
    II (a, b, c, d, t12, S41, 0x655b59c3); /* 53 */
    II (d, a, b, c, t03, S42, 0x8f0ccc92); /* 54 */
    II (c, d, a, b, t10, S43, 0xffeff47d); /* 55 */
    II (b, c, d, a, t01, S44, 0x85845dd1); /* 56 */
    II (a, b, c, d, t08, S41, 0x6fa87e4f); /* 57 */
    II (d, a, b, c, t15, S42, 0xfe2ce6e0); /* 58 */
    II (c, d, a, b, t06, S43, 0xa3014314); /* 59 */
    II (b, c, d, a, t13, S44, 0x4e0811a1); /* 60 */
    II (a, b, c, d, t04, S41, 0xf7537e82); /* 61 */
    II (d, a, b, c, t11, S42, 0xbd3af235); /* 62 */
    II (c, d, a, b, t02, S43, 0x2ad7d2bb); /* 63 */
    II (b, c, d, a, t09, S44, 0xeb86d391); /* 64 */

    Xa = ADD (Xa, init0);
    Ya = ADD (Ya, init0);
    Xb = ADD (Xb, init1);
    Yb = ADD (Yb, init1);
    Xc = ADD (Xc, init2);
    Yc = ADD (Yc, init2);
    Xd = ADD (Xd, init3);
    Yd = ADD (Yd, init3);

    *Xv0 = TRIM (Xa, 0);
    *Yv0 = TRIM (Ya, 0);
    *Xv1 = TRIM (Xb, 1);
    *Yv1 = TRIM (Yb, 1);
    *Xv2 = TRIM (Xc, 2);
    *Yv2 = TRIM (Yc, 2);

    MD5DEBUG ("Result is %08x %08x %08x\n",
              SWAP (FIRST (*Xv0)), SWAP (FIRST (*Xv1)), SWAP (FIRST (*Xv2)));
}


static void outline_MD5 (value_t * v)
{
    value_t a[3];
    a[0] = a[1] = a[2] = DIAG(0);
    inline_MD5 (v, v + 1, v + 2, a, a + 1, a + 2);
}


static int loop_MD5 (value_t * __restrict__ v,
                     uint64_t * __restrict__ iterations)
{
    value_t A = v[0];
    value_t B = v[1];
    value_t C = v[2];
    value_t D = v[3];
    value_t E = v[4];
    value_t F = v[5];
    int Xt;
    int Yt;

    do {
        inline_MD5 (&A, &B, &C, &D, &E, &F);
        ++*iterations;
        Xt = TEST (A);
        Yt = TEST (D);
    }
    while (__builtin_expect (Xt == 0, 1) && __builtin_expect (Yt == 0, 1));
    v[0] = A;
    v[1] = B;
    v[2] = C;
    v[3] = D;
    v[4] = E;
    v[5] = F;
    return Xt + Yt * 65536;
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

static struct timeval start_time;

static void store (value_t * v,
                   uint64_t * blocks,
                   uint64_t * starts,
                   uint64_t iteration,
                   int index)
{
    struct timeval current_time;
    gettimeofday (&current_time, NULL);
    uint64_t milliseconds = current_time.tv_sec - start_time.tv_sec;
    milliseconds *= 1000000;
    milliseconds += current_time.tv_usec - start_time.tv_usec;
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
            recorded_iterations, iteration * 1000 / milliseconds);
    fflush (NULL);

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

static void * main_loop (void * ignore)
{
    value_t V[6];
    V[0] = V[1] = V[2] = V[3] = V[4] = V[5] = DIAG (0);
    uint64_t blocks[WIDTH * 2];
    uint64_t starts[WIDTH * 2];
    uint64_t iteration = 0;

    pthread_mutex_lock (&lock);
    for (int i = 0; i != WIDTH; ++i) {
        recharge (V, blocks, starts, iteration, i);
        recharge (V + 3, blocks + 4, starts + 4, iteration, i);
    }
    pthread_mutex_unlock (&lock);

    while (true) {
        int t = loop_MD5 (V, &iteration);

        for (int i = 0; i != WIDTH; ++i) {
            if (TESTTEST (t, i)) {
                pthread_mutex_lock (&lock);
                store (V, blocks, starts, iteration, i);
                recharge (V, blocks, starts, iteration, i);
                pthread_mutex_unlock (&lock);
            }
            if (TESTTEST (t, i + 4)) {
                pthread_mutex_lock (&lock);
                store (V + 3, blocks + 4, starts + 4, iteration, i);
                recharge (V + 3, blocks + 4, starts + 4, iteration, i);
                pthread_mutex_unlock (&lock);
            }
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

    gettimeofday (&start_time, NULL);

    pthread_t th;
    pthread_create (&th, NULL, main_loop, NULL);
    main_loop (NULL);
}
