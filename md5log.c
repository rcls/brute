
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


static void run (uint32_t w0, uint32_t w1, uint32_t w2,
                 uint32_t w3, uint32_t w4, uint32_t w5)
{
    printf ("%08x %08x %08x %08x %08x %08x\n", w0, w1, w2, w3, w4, w5);

    uint32_t w[16];
    w[0] = w0;
    w[1] = w1;
    w[2] = w2;
    w[3] = w3;
    w[4] = w4;
    w[5] = w5;
    w[6] = 0;
    w[7] = 0;
    w[8] = 0;
    w[9] = 0;
    w[10] = 0;
    w[11] = 0;
    w[12] = 0;
    w[13] = 0;
    w[14] = 0x90;
    w[15] = 0;

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
    unsigned char bytes[32];
    uint32_t words[4] = { 0xe040a4f0, 0x7d4a91b5, 0x694f8475, 0x4e2443bc };
    //uint32_t words[4] = { 0, 0, 0, 0 };

    for (int i = 0; i != 18; ++i)
        bytes[i] = hexify (words[i/8] >> (4 * (i%8)));
    bytes[18] = 0x80;
    for (int i = 19; i != 31; ++i)
        bytes[i] = 0;

    uint32_t exp[8];
    for (int i = 0; i != 8; ++i)
        exp[i] = bytes[i*4] + bytes[i*4+1] * 256
            + bytes[i*4+2] * 65536 + bytes[i*4+3] * 16777216;

    run (exp[0], exp[1], exp[2], exp[3], exp[4], exp[5]);

    return 0;
}
