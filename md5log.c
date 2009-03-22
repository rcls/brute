
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// Do an md5 hash block, loging intermediate values.

//Note: All variables are unsigned 32 bits and wrap modulo 2^32 when calculating

typedef unsigned int u32;
static const int r[64] = {
    7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,
};


static u32 k[64];

static void init_k (void)
{
    //Use binary integer part of the sines of integers (Radians) as constants:
    for (int i = 0; i != 64; ++i)
        k[i] = (u32) (floor (fabs (sin (i + 1)) * 65536 * 65536));
}

static u32 leftrotate (u32 x, int r)
{
    return (x << r) + (x >> (32 - r));
}


static void run (u32 w0, u32 w1, u32 w2, u32 w3)
{
    u32 w[16];
    w[0] = w0;
    w[1] = w1;
    w[2] = w2;
    w[3] = w3;
    w[4] = 0x00000080;
    w[5] = 0;
    w[6] = 0;
    w[7] = 0;
    w[8] = 0;
    w[9] = 0;
    w[10] = 0;
    w[11] = 0;
    w[12] = 0;
    w[13] = 0;
    w[14] = 0x80;
    w[15] = 0;

    //Initialize variables:
    static const u32 h0 = 0x67452301;
    static const u32 h1 = 0xEFCDAB89;
    static const u32 h2 = 0x98BADCFE;
    static const u32 h3 = 0x10325476;

    u32 a = h0;
    u32 b = h1;
    u32 c = h2;
    u32 d = h3;

    //Main loop:
    for (int i = 0; i != 64; ++i) {
        printf ("%2u %08x %08x %08x %08x\n", i, a, b, c, d);
        u32 f;
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
        u32 temp = d;
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

int main()
{
    init_k();
//    run (0,0,0,0);
    run (0xe040a4f0, 0x7d4a91b5, 0x694f8475, 0x4e2443bc);

    return 0;
}
