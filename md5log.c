
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

// Do an md5 hash block, loging intermediate values.

//Note: All variables are unsigned 32 bits and wrap modulo 2^32 when calculating

static const int r[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,
};


static uint32_t k[64];

static void init_k (void)
{
    //Use binary integer part of the sines of integers (Radians) as constants:
    for (int i = 0; i != 64; ++i)
        k[i] = (uint32_t) (floor (fabs (sin (i + 1)) * 65536 * 65536));
}

static uint32_t leftrotate (uint32_t x, int r)
{
    return (x << r) + (x >> (32 - r));
}


static void run (const uint32_t w[16])
{
    printf ("%08x %08x %08x %08x %08x %08x %08x\n",
            w[0], w[1], w[2], w[3], w[4], w[5], w[6]);

    //Initialize variables:
    static const uint32_t h0 = 0x67452301;
    static const uint32_t h1 = 0xEFCDAB89;
    static const uint32_t h2 = 0x98BADCFE;
    static const uint32_t h3 = 0x10325476;

    uint32_t a = h0;
    uint32_t b = h1;
    uint32_t c = h2;
    uint32_t d = h3;

    //Main loop:
    for (int i = 0; i != 64; ++i) {
        printf ("%2u %08x %08x %08x %08x\n", i, a, b, c, d);
        uint32_t f;
        int g;
        if (i < 16) {
            f = (b & c) | ((~ b) & d);
            g = i;
        }
        else if (i < 32) {
            f = (d & b) | ((~ d) & c);
            g = (5 * i + 1) % 16;
        }
        else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        }
        else {
            f = c ^ (b | (~ d));
            g = (7 * i) % 16;
        }
        uint32_t temp = d;
        d = c;
        c = b;
        b = b + leftrotate (a + f + k[i] + w[g], r[i]);
        a = temp;
    }
    printf ("%2u %08x %08x %08x %08x\n", 64, a, b, c, d);
    //Add this chunk's hash to result so far:
    a += h0;
    b += h1;
    c += h2;
    d += h3;
    printf ("   %08x %08x %08x %08x\n", a, b, c, d);
}

static unsigned char hexify (int n)
{
    n &= 15;
    if (n < 10)
        return n + '0';
    else
        return n - 10 + 'a';
}

int main()
{
    init_k();
//    run (0,0,0,0);
    unsigned char bytes[64];
    uint32_t words[4] = { 0xe040a4f0, 0x7d4a91b5, 0x694f8475, 0x4e2443bc };
    //uint32_t words[4] = { 0, 0, 0, 0 };

    for (int i = 0; i != 24; ++i)
        bytes[i] = hexify (words[i/8] >> (4 * (i%8)));
    bytes[24] = 0x80;
    for (int i = 25; i != 64; ++i)
        bytes[i] = 0;

    uint32_t w[16];
    for (int i = 0; i != 14; ++i)
        w[i] = bytes[i*4] + bytes[i*4+1] * 256
            + bytes[i*4+2] * 65536 + bytes[i*4+3] * 16777216;
    w[14] = 0xc0;
    w[15] = 0;

    run (w);

    return 0;
}
