/* Simulate parallel brute forcing of a hash.  We look for collisions
 * on the first few bits of MD5.  */

/* In a real parallel implementation, the calls to the function unit
 * below would be farmed out to multiple processors.  This is only a
 * simulation so we only do one call at a time...  */

/* Compile with something like
 *
 * gcc -O3 -Wall -Winline -Werror -std=gnu99 -fomit-frame-pointer -march=i686 -mcpu=i686 brute.c -o brute
 *
 * Your compiler better be good at constant folding inline functions
 * and static const variables - you may wanna check the assembly...
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Bits of hash to take.  The hash function we compute collisions for
 * is the first BITS bits of the md5 value.  The program will take
 * around 1<<(BITS/2) microseconds to run.  */
#define BITS 52

/* Iterations per processing unit.  1<<(BITS/2-5) is about appropriate.  */
static const int ITERATE = 1<<22;

/* Number of items to keep per unit.  Not critical but don't want it
 * too small.  */
static const int KEEP = 16;

/* Number of complete bytes in hash output.  */
static const int WHOLE = BITS / 8;
/* Number of bytes of hash to store.  */
#define STORE ((BITS + 7) / 8)

/* Bit mask for any final partial byte.  */
static const int MASK = 256 - (1 << (8 - (BITS & 7)));

/* When we say inline we mean it --- we're relying heavily on constant
 * folding functions such as word() below.  */
#define inline __attribute__ ((always_inline))

static inline unsigned int hexc (unsigned int n)
{
    unsigned int r = n + '0';
    if (n > 9)
	r += 'a' - '0' - 10;
    return r;
}

static inline unsigned int byte (int n, const unsigned char in[STORE])
{
    if (n < STORE * 2)
	/* Bytes are big endian...  */
	if ((n & 1) == 0)
	    return hexc (in[n / 2] >> 4);
	else
	    return hexc (in[n / 2] & 15);
    else if (n == STORE * 2)
	return '\n';
    else if (n == STORE * 2 + 1)
	return 0x80;
    else
	return 0;
}

static inline unsigned int word (int n, const unsigned char in[STORE])
{
    n *= 4;
    return byte (n, in) + (byte (n + 1, in) << 8) + (byte (n + 2, in) << 16) + (byte (n + 3, in) << 24);
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
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.  */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
 * Rotation is separate from addition to prevent recomputation.  */
#define FF(a, b, c, d, x, s, ac) {				\
	(a) += F ((b), (c), (d)) + (x) + (unsigned int)(ac);	\
	(a) = ROTATE_LEFT ((a), (s));				\
        (a) += (b);						\
    }
#define GG(a, b, c, d, x, s, ac) {				\
	(a) += G ((b), (c), (d)) + (x) + (unsigned int)(ac);	\
	(a) = ROTATE_LEFT ((a), (s));				\
	(a) += (b);						\
    }
#define HH(a, b, c, d, x, s, ac) {				\
	(a) += H ((b), (c), (d)) + (x) + (unsigned int)(ac);	\
	(a) = ROTATE_LEFT ((a), (s));				\
	(a) += (b);						\
    }
#define II(a, b, c, d, x, s, ac) {				\
	(a) += I ((b), (c), (d)) + (x) + (unsigned int)(ac);	\
	(a) = ROTATE_LEFT ((a), (s));				\
	(a) += (b);						\
    }

/* Generate md5 hash, storing the first 'whole' complete bytes and
 * masking the next byte by 'mask'.  Gets specialised to what we want.  */
static inline void do_MD5 (unsigned char * out, int whole, int mask,
			   const unsigned char in[STORE])
{
    const unsigned int init0 = 0x67452301;
    const unsigned int init1 = 0xefcdab89;
    const unsigned int init2 = 0x98badcfe;
    const unsigned int init3 = 0x10325476;

    unsigned int a = init0;
    unsigned int b = init1;
    unsigned int c = init2;
    unsigned int d = init3;

    /* Round 1 */
    const unsigned int xx00 = word (0, in);
    FF (a, b, c, d, xx00, S11, 0xd76aa478); /* 1 */
    const unsigned int xx01 = word (1, in);
    FF (d, a, b, c, xx01, S12, 0xe8c7b756); /* 2 */
    const unsigned int xx02 = word (2, in);
    FF (c, d, a, b, xx02, S13, 0x242070db); /* 3 */
    const unsigned int xx03 = word (3, in);
    FF (b, c, d, a, xx03, S14, 0xc1bdceee); /* 4 */
    const unsigned int xx04 = word (4, in);
    FF (a, b, c, d, xx04, S11, 0xf57c0faf); /* 5 */
    const unsigned int xx05 = word (5, in);
    FF (d, a, b, c, xx05, S12, 0x4787c62a); /* 6 */
    const unsigned int xx06 = word (6, in);
    FF (c, d, a, b, xx06, S13, 0xa8304613); /* 7 */
    const unsigned int xx07 = word (7, in);
    FF (b, c, d, a, xx07, S14, 0xfd469501); /* 8 */
    const unsigned int xx08 = word (8, in);
    FF (a, b, c, d, xx08, S11, 0x698098d8); /* 9 */
    const unsigned int xx09 = word (9, in);
    FF (d, a, b, c, xx09, S12, 0x8b44f7af); /* 10 */
    const unsigned int xx10 = word (10, in);
    FF (c, d, a, b, xx10, S13, 0xffff5bb1); /* 11 */
    const unsigned int xx11 = word (11, in);
    FF (b, c, d, a, xx11, S14, 0x895cd7be); /* 12 */
    const unsigned int xx12 = word (12, in);
    FF (a, b, c, d, xx12, S11, 0x6b901122); /* 13 */
    const unsigned int xx13 = word (13, in);
    FF (d, a, b, c, xx13, S12, 0xfd987193); /* 14 */
    const unsigned int xx14 = STORE * 16 + 8; /* Account for hex expansion.  */
    FF (c, d, a, b, xx14, S13, 0xa679438e); /* 15 */
    const unsigned int xx15 = 0;
    FF (b, c, d, a, xx15, S14, 0x49b40821); /* 16 */

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

    a += init0;
    b += init1;
    c += init2;
    d += init3;

    /* n, whole and mask are always constants, so the expressions
     * below constant fold down to a shift and mask.  */
#define SAVE_SELECT(n) (n < 4 ? a : (n < 8 ? b : (n < 12 ? c : d)))
#define SAVE_BYTE(n) ((SAVE_SELECT(n) >> (8 * (n & 3))) & 255)
#define SAVE(n) (n < whole ? out[n] = SAVE_BYTE(n) : ((mask && n == whole) ? out[n] = SAVE_BYTE(n) & MASK : 0))
    SAVE(0);
    SAVE(1);
    SAVE(2);
    SAVE(3);
    SAVE(4);
    SAVE(5);
    SAVE(6);
    SAVE(7);
    SAVE(8);
    SAVE(9);
    SAVE(10);
    SAVE(11);
    SAVE(12);
    SAVE(13);
    SAVE(14);
    SAVE(15);
}

static void md5_short (unsigned char out[STORE], const unsigned char in[STORE])
{
    do_MD5 (out, WHOLE, MASK, in);
}

static void md5_long (unsigned char out[16], const unsigned char in[STORE])
{
    do_MD5 (out, 16, 0, in);
}

typedef struct Item
{
    int start;
    int count;
    unsigned char out[STORE];
} Item;

static void prep (unsigned char * p, int start)
{
    memset (p, 0, STORE);
    for (; start; start >>= 8)
	*p++ = start & 255;
}

/* Do a unit of work.  Iterate md5 ITERATE times, starting from start.
 * Keep the KEEP smallest hash results, returning them in U.  */
static void unit (Item U[KEEP], int start)
{
    unsigned char current[STORE];
    prep (current, start);

    for (int i = 0; i != KEEP; ++i)
	memset (U[i].out, 0xff, STORE);

    for (int count = 1; count != ITERATE + 1; ++count) {
	md5_short (current, current);

	/* Quick case, don't keep.  */
	if (__builtin_expect (current[0] != 0, 1))
	    continue;

	if (memcmp (current, U[0].out, STORE) > 0)
	    continue;

	/* Put this item into the unit array, keeping in decreasing
	 * order of hash.  */
	int p;
	for (p = 0; p != KEEP - 1; ++p) {
	    if (memcmp (current, U[p+1].out, STORE) > 0)
		break;		/* Not at p+1.  */
	    U[p] = U[p+1];
	}
	/* Put into position p.  */
	U[p].start = start;
	U[p].count = count;
	memcpy (U[p].out, current, STORE);
    }
}

/* Print a block of bytes in hex.  */
static void print_bytes (FILE * f, const unsigned char * p, int n)
{
    for (int i = 0; i != n; ++i)
	fprintf (f, "%02x", p[i]);
}

/* Do the catch up iteration for extracting the final collision.
 * Returns false if this turns out to be a pre-image, true if it's OK.  */
static bool adjustment (unsigned char buf[STORE], int count,
			const unsigned char other[STORE])
{
    if (count <= 0)
	return true;

    unsigned char pre[STORE];
    memcpy (pre, buf, STORE);

    while (--count)		/* Loop does all but one iteration.  */
	md5_short (pre, pre);

    md5_short (buf, pre);
    if (memcmp (buf, other, STORE) != 0)
	return true;

    /* We have a pre-image not a collision.  Report it.  */
    unsigned char hash[16];
    md5_long (hash, pre);

    printf ("Preimage:\n");
    print_bytes (stdout, pre, STORE);
    printf ("\t");
    print_bytes (stdout, other, STORE);
    printf ("\t");
    print_bytes (stdout, hash, 16);
    printf ("\n");

    return false;
}

/* Produce a report on the collision.  Return false if it's actually a
 * preimage, true if it's OK.  */
static bool report (int startA, int countA, int startB, int countB)
{
    unsigned char A0[STORE];
    unsigned char B0[STORE];
    unsigned char A1[STORE];
    unsigned char B1[STORE];
    /* Search for the actual collision.  */
    prep (A0, startA);
    prep (B0, startB);
    /* ... adjust the starting points for the search, checking for a
     * pre-image.  */
    if (!adjustment (B0, countB - countA, A0))
	return false;
    if (!adjustment (A0, countA - countB, B0))
	return false;

    /* Now search, iterating both items together.  */
    while (1) {
	md5_short (A1, A0);
	md5_short (B1, B0);
	if (memcmp (A1, B1, STORE) == 0)
	    goto in_zero;
	md5_short (A0, A1);
	md5_short (B0, B1);
	if (memcmp (A0, B0, STORE) == 0)
	    break;
    }
    memcpy (A0, A1, STORE);
    memcpy (B0, B1, STORE);

in_zero:
    ;
    /* Print it out.  */
    unsigned char hashA[16];
    unsigned char hashB[16];
    md5_long (hashA, A0);
    md5_long (hashB, B0);

    print_bytes (stdout, A0, STORE);
    printf ("\t");
    print_bytes (stdout, hashA, 16);
    printf ("\n");

    print_bytes (stdout, B0, STORE);
    printf ("\t");
    print_bytes (stdout, hashB, 16);
    printf ("\n");

    int n;
    for (n = 0; n < 128; ++n)
	if ((hashA[n / 8] ^ hashB[n / 8]) & (128 >> (n & 7)))
	    break;
    printf ("\t%i bits of collision.\n", n);

    FILE * p = popen ("md5sum", "w");
    if (p) {
	print_bytes (p, A0, STORE);
	fprintf (p, "\n");
	fclose (p);
    }
    p = popen ("md5sum", "w");
    if (p) {
	print_bytes (p, B0, STORE);
	fprintf (p, "\n");
	fclose (p);
    }

    return true;
}    

int main()
{
    Item * items = NULL;

    for (int n = 0;; ++n)
    {
	items = realloc (items, (n + 1) * KEEP * sizeof (Item));
	if (items == NULL) {
	    fprintf (stderr, "brute: malloc failed\n");
	    exit (1);
	}
	unit (&items[n * KEEP], n);
	printf ("Unit %i\n", n);
	/* Putting the items in a hash would be quicker...  But this
	 * shouldn't be speed critical anyway.  */
	for (int i = n * KEEP; i != (n + 1) * KEEP; ++i) {
	    if (items[i].out[0] == 255)
		continue;	/* Unlikely unless ITERATE is too small.  */
	    for (int j = 0; j != i; ++j)
		if (memcmp (items[i].out, items[j].out, STORE) == 0) {
		    printf ("%i\t%i\n%i\t%i\t",
			    items[i].start, items[i].count,
			    items[j].start, items[j].count);
		    print_bytes (stdout, items[i].out, STORE);
		    printf ("\n");
		    if (report (items[i].start, items[i].count,
				items[j].start, items[j].count))
			exit (0);
		}
	}
    }
}
