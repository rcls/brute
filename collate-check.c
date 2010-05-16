
/* Compile with something like
 *
 * gcc -O3 -O2 -flax-vector-conversions -msse -msse2 -march=athlon64 -mtune=athlon64 -Wall -Winline -std=gnu99 -fomit-frame-pointer brute.c -msse2 -o brute
 *
 * Your compiler better be good at constant folding inline functions
 * and static const variables - you may wanna check the assembly...
 */

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
//#include <sys/time.h>

#include "jtag-io.h"

/* When we say inline we mean it --- we're relying heavily on constant
 * folding functions such as word() below.  */
#define INLINE __attribute__ ((always_inline))

static INLINE uint32_t SWAP (uint32_t a)
{
    return __builtin_bswap32 (a);
}


/* Bits of hash to take.  */
#define BITS 96


#define DEBUG(...) fprintf (stderr, __VA_ARGS__)
#define NODEBUG(...) while (0) fprintf (stderr, __VA_ARGS__)


typedef unsigned __attribute__ ((vector_size (16))) value_t;
static INLINE value_t DIAG (uint32_t a)
{
    return (value_t) { a, a, a, a };
}


static INLINE uint32_t FIRST (value_t A)
{
    typedef unsigned u128 __attribute__ ((mode (TI)));
    return (u128) A;
}


static inline value_t ANDN (value_t a, value_t b)
{
    return __builtin_ia32_pandn128 (a, b);
}


#define RIGHT(a,n) __builtin_ia32_psrldi128 (a, n)
#define LEFT(a,n) __builtin_ia32_pslldi128 (a, n)


static INLINE uint32_t EXTRACT (value_t v, int index)
{
    if (__builtin_constant_p (index) && index == 0)
        return FIRST (v);

    switch (index) {
    case 0:
        return FIRST (v);
    case 1:
        return FIRST (__builtin_ia32_pshufd (v, 0x55));
    case 2:
        return FIRST (__builtin_ia32_pshufd (v, 0xaa));
    case 3:
        return FIRST (__builtin_ia32_pshufd (v, 0xff));
    }
    abort();
}


static value_t INSERT (value_t v, int index, uint32_t vv)
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


static INLINE void BYTE_INTERLEAVE (value_t * __restrict__ lo,
                                    value_t * __restrict__ hi)
{
#define BIDEBUG NODEBUG
    BIDEBUG ("I0: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             EXTRACT (*lo, 0),
             EXTRACT (*lo, 1),
             EXTRACT (*lo, 2),
             EXTRACT (*lo, 3),
             EXTRACT (*hi, 0),
             EXTRACT (*hi, 1),
             EXTRACT (*hi, 2),
             EXTRACT (*hi, 3));

    // lo 8 bytes interleaved into 16 bytes.
    value_t lo1 = __builtin_ia32_punpcklbw128 (*lo, *hi);
    // hi 8 bytes interleaved into 16 bytes.
    value_t hi1 = __builtin_ia32_punpckhbw128 (*lo, *hi);

    BIDEBUG ("I1: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             EXTRACT (lo1, 0),
             EXTRACT (lo1, 1),
             EXTRACT (lo1, 2),
             EXTRACT (lo1, 3),
             EXTRACT (hi1, 0),
             EXTRACT (hi1, 1),
             EXTRACT (hi1, 2),
             EXTRACT (hi1, 3));

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
             EXTRACT (lo2, 0),
             EXTRACT (lo2, 1),
             EXTRACT (lo2, 2),
             EXTRACT (lo2, 3),
             EXTRACT (hi2, 0),
             EXTRACT (hi2, 1),
             EXTRACT (hi2, 2),
             EXTRACT (hi2, 3));

    // Now we want
    // lo.0 = lo2.0, hi.0 = lo2.2,
    // lo.1 = lo2.1, hi.1 = lo2.3,
    // lo.2 = hi2.0, hi.2 = hi2.2,
    // lo.3 = hi2.1, lo.3 = hi2.3
    *lo = __builtin_ia32_punpcklqdq128 (lo2, hi2);
    *hi = __builtin_ia32_punpckhqdq128 (lo2, hi2);

    BIDEBUG ("I3: %08x %08x %08x %08x  %08x %08x %08x %08x\n",
             EXTRACT (*lo, 0),
             EXTRACT (*lo, 1),
             EXTRACT (*lo, 2),
             EXTRACT (*lo, 3),
             EXTRACT (*hi, 0),
             EXTRACT (*hi, 1),
             EXTRACT (*hi, 2),
             EXTRACT (*hi, 3));
}


// Convert a collection of nibbles to ASCII values.
static INLINE value_t ASCIIFY (value_t nibbles)
{
    value_t adjust = ((value_t) __builtin_ia32_pcmpgtb128 (
                          nibbles, DIAG (0x09090909)))
        & DIAG (0x27272727);
/*     value_t result = __builtin_ia32_paddb128 (nibbles, DIAG (0x30303030)); */
/*     return result + adjust; */
    return nibbles + DIAG(0x30303030) + adjust;
}


static inline value_t AND (value_t a, value_t b) { return a & b; }
static inline value_t OR  (value_t a, value_t b) { return a | b; }
static inline value_t XOR (value_t a, value_t b) { return a ^ b; }
static inline value_t ADD (value_t a, value_t b) { return a + b; }
static inline value_t NOT (value_t a) { return ~a; }

#ifndef LEFT
static INLINE value_t LEFT (value_t a, int count) { return a << count; }
#endif
#ifndef RIGHT
static INLINE value_t RIGHT (value_t a, int count) { return a >> count; }
#endif

static INLINE value_t ROTATE_LEFT (value_t a, unsigned count)
{
    return OR (LEFT (a, count), RIGHT (a, 32 - count));
}

static INLINE void EXPAND (value_t v,
                           value_t * __restrict__ V0,
                           value_t * __restrict__ V1)
{
    *V0 = AND (v, DIAG (0x0f0f0f0f));
    *V1 = AND (RIGHT(v, 4), DIAG (0x0f0f0f0f));
    BYTE_INTERLEAVE (V0, V1);
    *V0 = ASCIIFY (*V0);
    *V1 = ASCIIFY (*V1);
#define EXDEBUG NODEBUG
    EXDEBUG ("Expand: %08x %08x %08x %08x\n"
             "        %08x %08x %08x %08x\n"
             "        %08x %08x %08x %08x\n",
             EXTRACT (v, 0),
             EXTRACT (v, 1),
             EXTRACT (v, 2),
             EXTRACT (v, 3),

             EXTRACT (*V0, 0),
             EXTRACT (*V0, 1),
             EXTRACT (*V0, 2),
             EXTRACT (*V0, 3),

             EXTRACT (*V1, 0),
             EXTRACT (*V1, 1),
             EXTRACT (*V1, 2),
             EXTRACT (*V1, 3));
}

#define SPLIT(v,V0,V1) value_t V0; value_t V1; EXPAND (v, &V0, &V1);


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

#define GENERATE(FUN,a, b, c, d, x, s, ac) {                    \
	(a) = A4 ((a), FUN ((b), (c), (d)), (x), DIAG (ac));	\
	(a) = ROTATE_LEFT ((a), (s));                           \
        (a) = ADD ((a), (b));                                   \
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


static uint64_t iterate_MD5 (value_t * __restrict v, uint64_t n)
{
    uint64_t remain = n;
    value_t check;
    do {
        inline_MD5 (v, v + 1, v + 2);
        check = AND (*v, DIAG (0x3fffffff));
        --remain;
    }
    while (remain
           && EXTRACT (check, 0)
           && EXTRACT (check, 1)
           && EXTRACT (check, 2)
           && EXTRACT (check, 3));
    return n - remain;
}


typedef struct result_t {
    uint64_t clock;
    const struct result_t * channel_prev;
    double logprob;
    int pipe;
    uint32_t data[3];
} result_t;


static const result_t * current[PIPELINES * STAGES];
static result_t ** results = NULL;
static size_t result_count = 0;
static uint64_t cycles = 0;
static uint64_t session_start_cycles = 0;
static int global_result_index = 0;

static void read_log_result()
{
    result_t * r = malloc (sizeof (result_t));
    if (r == NULL)
        printf_exit ("Out of memory (result)\n");
    if (scanf (" %lu %u %x %x %x ",
               &r->clock, &r->pipe, &r->data[0], &r->data[1], &r->data[2])
        != 5) {
        fprintf (stderr, "Bogus R line in log file\n");
        free (r);
        return;
    }

    int slot = r->pipe * STAGES + r->clock % STAGES;
    if (r->data[0] & TRIGGER_MASK)
        r->channel_prev = NULL;
    else
        r->channel_prev = current[slot];
    current[slot] = r;

    if (r->channel_prev != NULL) {
        results = realloc (results, ++result_count * sizeof (result_t *));
        if (results == NULL)
            printf_exit ("Out of memory (result array)\n");
        results[result_count - 1] = r;
        cycles += (r->clock - r->channel_prev->clock) / STAGES;
    }
}


static void read_log_hit()
{
#define H3 "%*x %*x %*x "
    if (scanf (" %*u %*u %*u %*u %*u " H3 H3 H3 H3) != 0)
        fprintf (stderr, "Bogus H line in log file\n");
}


static void read_log_error (void)
{
    if (scanf (" %*u %*u %*u %*u %*x %*x %*x %*x %*x %*x ") != 0)
        fprintf (stderr, "Bogus E line in log file\n");
}


static void print_session_length (const char * tag, uint64_t sc)
{
    if (sc == 0)
        return;

    unsigned int secs = sc / (PIPELINES * FREQ);
    printf ("%s %6u seconds (%u %02u:%02u:%02u.%06lu + %3lu)\n",
            tag,
            secs,
            secs / 86400,
            secs / 3600 % 24,
            secs / 60 % 60,
            secs % 60,
            sc / (PIPELINES * FREQ / 1000000) % 1000000,
            sc % (PIPELINES * FREQ / 1000000));
}


static void read_session (void)
{
    print_session_length ("Session", cycles - session_start_cycles);

    time_t t;
    int stages;
    int pipelines;
    if (scanf (" %lu %u %u ", &t, &stages, &pipelines) != 3) {
        fprintf (stderr, "Bogus S line in log file\n");
        return;
    }

    char tt[30] = "";
    if (strftime (tt, 30, "%c", localtime (&t)) == 0)
        tt[0] = 0;

    bool started = false;
    for (int i = 0; i != PIPELINES * STAGES; ++i) {
        if (current[i] != NULL) {
            if (!started && i != 0)
                fprintf (stderr, "Up to stage %i, pipe %i empty\n",
                         (i - 1) % STAGES, (i - 1) / STAGES);
            started = true;
        }
        else if (started)
            fprintf (stderr, "Stage %i, pipe %i empty\n",
                     i % STAGES, i / STAGES);
    }

    if (stages != STAGES)
        printf_exit ("Session %s stage mismatch %u != %u\n",
                     tt, stages, STAGES);

    if (pipelines != PIPELINES)
        printf_exit ("Session %s pipeline mismatch %u != %u\n",
                     tt, pipelines, PIPELINES);

    printf ("Reading session started %s.\n", tt);

    session_start_cycles = cycles;

    memset (current, 0, sizeof (current));
}


static void read_log_file (void)
{
    while (true) {
        int c = getchar();
        switch (c) {
        case 'R':
            read_log_result();
            break;
        case 'H':
            read_log_hit();
            break;
        case 'E':
            read_log_error();
            break;
        case 'S':
            read_session();
            break;
        case EOF:
            return;
        default:
            fprintf (stderr, "Ignoring bogus char %02x in log file", c);
            break;
        }
    }
}

static uint64_t gapof (const result_t * r)
{
    return (r->clock - r->channel_prev->clock) / STAGES;
}


static const result_t * finish (value_t * __restrict vv,
                                int i,
                                uint64_t * __restrict remain,
                                const result_t * r)
{
    if (r == NULL)
        ;
    else if (EXTRACT (vv[0], i) == r->data[0] &&
             EXTRACT (vv[1], i) == r->data[1] &&
             EXTRACT (vv[2], i) == r->data[2])
        printf ("Verified %lu %c[%lu]\n",
                r->clock, 'A' + r->pipe, r->clock % STAGES);
    else
        printf ("\aFAILED %lu %c[%lu] after %lu iterations\n"
                "Got %08x %08x %08x\n"
                "Exp %08x %08x %08x\n",
                r->clock, 'A' + r->pipe, r->clock % STAGES,
                gapof (r),
                EXTRACT (vv[0], i), EXTRACT (vv[1], i),
                EXTRACT (vv[2], i),
                r->data[0], r->data[1], r->data[2]);

    int index = __sync_add_and_fetch (&global_result_index, 1) - 1;
    if (index >= result_count)
        index = random() % result_count;

    r = results[index];
    vv[0] = INSERT (vv[0], i, r->channel_prev->data[0]);
    vv[1] = INSERT (vv[1], i, r->channel_prev->data[1]);
    vv[2] = INSERT (vv[2], i, r->channel_prev->data[2]);
    *remain = gapof (r);
    printf ("Start %lu %c[%lu], %lu iterations\n",
            r->clock, 'A' + r->pipe, r->clock % STAGES,
            *remain);
    return r;
}

static void * check_thread (void * unused)
{
    value_t vv[3];
    const result_t * checking[4] = { NULL, NULL, NULL, NULL };
    uint64_t remain[4] = { 0, 0, 0, 0 };
    uint64_t iterations = 0;
    time_t start = time (NULL);
    uint64_t total = 0;
    while (true) {
        for (int i = 0; i != 4; ++i) {
            const result_t * r = checking[i];
            remain[i] -= iterations;
            if (remain[i] == 0)
                checking[i] = finish (vv, i, &remain[i], r);
            else if ((EXTRACT (vv[0], i) & 0x3fffffff) == 0)
                printf ("R %lu %i %8x %8x %8x\n",
                        r->clock - STAGES * remain[i],
                        r->pipe,
                        EXTRACT (vv[0], i),
                        EXTRACT (vv[1], i),
                        EXTRACT (vv[2], i));
        }

        iterations = remain[0];
        for (int i = 1; i != 4; ++i)
            if (remain[i] < iterations)
                iterations = remain[i];

        iterations = iterate_MD5 (vv, iterations);
        total += iterations;

        time_t elapsed = time (NULL) - start;
        fprintf (stderr, "Iterations: %lu, seconds %lu, per-sec %lu\n",
                 total, elapsed, total / elapsed);
    }
    return NULL;
}


static int clock_order (const void * AA, const void * BB)
{
    const result_t * const * A = AA;
    const result_t * const * B = BB;

    uint64_t cA = (*A)->clock;
    uint64_t cB = (*B)->clock;
    if (cA < cB)
        return -1;
    if (cA > cB)
        return 1;
    return 0;
}


static int logprob_order (const void * AA, const void * BB)
{
    const result_t * const * A = AA;
    const result_t * const * B = BB;

    double pA = (*A)->logprob;
    double pB = (*B)->logprob;
    if (pA < pB)
        return -1;
    if (pA > pB)
        return 1;
    return 0;
}


static inline double prob (uint64_t start, uint64_t end)
{
    const double rate = 1.0 / (1 << 30) / STAGES;
    double expect = (end - start) * rate;
    return log1p (expect) - expect;
}


int main (int argc, char ** argv)
{
    // Line buffer all output.
    setvbuf (stdout, NULL, _IOLBF, 0);

    read_log_file();

    print_session_length ("Session", cycles - session_start_cycles);
    print_session_length ("Total", cycles);

    qsort (results, result_count, sizeof (result_t *), clock_order);

    // Fill in the logprobs for each range.
    for (int i = 0; i != result_count; ++i) {
        double logprob = 0;
        uint64_t c_start = results[i]->channel_prev->clock;
        uint64_t c_end = results[i]->clock;
        bool seen[PIPELINES][STAGES];
        memset (seen, 0, sizeof (seen));
        int virgin = PIPELINES * STAGES;
        for (int j = i - 1; j >= 0 && results[j]->clock > c_start; --j) {
            const result_t * r = results[j];
            uint64_t c_hi = r->clock;
            uint64_t c_lo = r->channel_prev->clock;
            if (c_lo < c_start)
                c_lo = c_start;

            if (!seen[r->pipe][r->clock % STAGES]) {
                seen[r->pipe][r->clock % STAGES] = true;
                --virgin;
                logprob += prob (c_hi, c_end);
            }
            logprob += prob (c_lo, c_hi);
        }

        results[i]->logprob = logprob + virgin * prob (c_start, c_end);
    }

    qsort (results, result_count, sizeof (result_t *), logprob_order);

#if 1
    for (int i = 0; i != result_count; ++i)
        printf ("%16lu [%c] %11lu %f\n",
                results[i]->clock, results[i]->pipe + 'A',
                gapof (results[i]), results[i]->logprob);
#endif

    if (argc <= 1)
        return 0;

    pthread_t t;
    pthread_create (&t, NULL, check_thread, NULL);
    pthread_create (&t, NULL, check_thread, NULL);
    pthread_create (&t, NULL, check_thread, NULL);
    check_thread (NULL);
}
