/* Simulate parallel brute forcing of a hash.  We look for collisions
 * on the first few bits of MD5.  */

/* In a real parallel implementation, the calls to the function unit
 * below would be farmed out to multiple processors.  This is only a
 * simulation so we only do one call at a time...  */

#include <assert.h>
#include <openssl/md5.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/* Bits of hash to take.  The hash function we compute collisions for
 * is the first BITS bits of the md5 value.  */
#define BITS 52

/* Iterations per processing unit.  */
#define ITERATE (1<<22)

/* Number of items to keep per unit.  Not critical but don't want it
 * too small.  */
#define KEEP 16

/* Whole bytes to compare.  */
#define WHOLE (BITS/8)
/* Left over bits to compare on the partial byte.  */
#define LEFT (BITS&7)
/* Bytes of hash to store.  */
#define STORE (WHOLE + (LEFT != 0))


typedef struct Item
{
    int start;
    int count;
    unsigned char out[STORE];
} Item;

static inline void lastcp (unsigned char out[STORE], const unsigned char in[STORE])
{
    if (LEFT)
	out[WHOLE] = in[WHOLE] & (0xff << (8 - LEFT));
}

static inline unsigned char hexc (int n)
{
    return n > 9 ? n - 10 + 'a' : n + '0';
}

/* MD5 on a block of memory; we expand the block into newline
 * terminated hex first, just so you can do echo foo|md5sum.  */
static inline void do_MD5 (unsigned char res[MD5_DIGEST_LENGTH],
			   const unsigned char in[STORE])
{
    unsigned char buf[STORE * 2 + 1];
    for (int i = 0; i != STORE; ++i) {
	buf[i * 2] = hexc (in[i] >> 4);
	buf[i * 2 + 1] = hexc (in[i] & 0xf);
    }
    buf[STORE * 2] = '\n';
    MD5_CTX ctx;
    MD5_Init (&ctx);
    MD5_Update (&ctx, buf, STORE * 2 + 1);
    MD5_Final (res, &ctx);
}

/* Turn the integer start into a block of bytes to hash.  */
static void prep (unsigned char * p, int start)
{
    memset (p, 0, STORE);
    for (; start; start >>= 8)
	*p++ = start & 0xff;
}

/* Do a unit of work.  */
static void unit (Item U[KEEP], int start)
{
    unsigned char current[MD5_DIGEST_LENGTH];
    prep (current, start);

    for (int i = 0; i != KEEP; ++i)
	memset (U[i].out, 0xff, STORE);

    for (int count = 1; count != ITERATE + 1; ++count) {
	do_MD5 (current, current);
	lastcp (current, current);

	/* Quick case, don't keep.  */
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

/* Print a block of bytes.  */
static void print_bytes (const unsigned char * b, int n)
{
    for (int i = 0; i != n; ++i)
	printf ("%02x", b[i]);
}

/* Do the catch up iteration for extracting the final collision.
 * Returns false if this turns out to be a pre-image, true if it's OK.  */
static bool adjustment (unsigned char buf[STORE], int count,
			const unsigned char other[STORE])
{
    if (count <= 0)
	return true;

    unsigned char hash[MD5_DIGEST_LENGTH];
    memcpy (hash, buf, STORE);

    while (--count) {
	do_MD5 (hash, hash);
	lastcp (hash, hash);
    }
    memcpy (buf, hash, STORE);
    do_MD5 (hash, buf);
    if (memcmp (hash, other, WHOLE) != 0 ||
	(LEFT != 0 && ((hash[WHOLE] ^ other[WHOLE]) & (0xff << (8-LEFT))) != 0))
    {
	memcpy (buf, hash, WHOLE);
	lastcp (buf, hash);
	return true;
    }

    /* We have a pre-image not a collision.  Report it.  */
    printf ("Preimage:\n");
    print_bytes (buf, STORE);
    printf ("\t");
    print_bytes (other, STORE);
    printf ("\t");
    print_bytes (hash, MD5_DIGEST_LENGTH);
    printf ("\n");
    return false;
}

/* Produce a report on the collision.  Return false if it's actually a
 * preimage, true if it's OK.  */
static bool report (int startA, int countA, int startB, int countB)
{
    unsigned char bufA[STORE];
    unsigned char bufB[STORE];
    unsigned char nA[MD5_DIGEST_LENGTH];
    unsigned char nB[MD5_DIGEST_LENGTH];
    /* Search for the actual collision.  */
    prep (bufA, startA);
    prep (bufB, startB);
    /* ... adjust the starting points for the search, checking for a
     * pre-image.  */
    if (!adjustment (bufB, countB - countA, bufA))
	return false;
    if (!adjustment (bufA, countA - countB, bufB))
	return false;

    /* Now search, iterating both items together.  */
    while (1) {
	do_MD5 (nA, bufA);
	do_MD5 (nB, bufB);
	if (memcmp (nA, nB, WHOLE) == 0 &&
	    (LEFT == 0 || ((nA[WHOLE] ^ nB[WHOLE]) & (0xff << (8-LEFT))) == 0))
	    break;
	memcpy (bufA, nA, WHOLE);
	lastcp (bufA, nA);
	memcpy (bufB, nB, WHOLE);
	lastcp (bufB, nB);
    }
    /* Print it out.  */
    print_bytes (bufA, STORE);
    printf ("\t");
    print_bytes (nA, MD5_DIGEST_LENGTH);
    printf ("\n");

    print_bytes (bufB, STORE);
    printf ("\t");
    print_bytes (nB, MD5_DIGEST_LENGTH);
    printf ("\n");

    int n;
    for (n = 0; n < 8 * MD5_DIGEST_LENGTH; ++n)
	if ((nA[n / 8] ^ nB[n / 8]) & (0x80 >> (n % 8)))
	    break;
    printf ("\t%i bits of collision.\n", n);
    return true;
}    

int main()
{
    Item * items = NULL;
    int i, j;

    for (int n = 0;; ++n)
    {
	items = realloc (items, (n + 1) * KEEP * sizeof (Item));
	assert (items != NULL);
	unit (&items[n * KEEP], n);
	printf ("Unit %i\n", n);
	/* Putting the items in a hash would be quicker...  */
	for (i = n * KEEP; i != (n + 1) * KEEP; ++i)
	    for (j = 0; j != i; ++j)
		if (memcmp (items[i].out, items[j].out, STORE) == 0) {
		    printf ("%i\t%i\n%i\t%i\t",
			    items[i].start, items[i].count,
			    items[j].start, items[j].count);
		    print_bytes (items[i].out, STORE);
		    printf ("\n");
		    if (report (items[i].start, items[i].count,
				items[j].start, items[j].count))
			return 0;
		}
    }
}
