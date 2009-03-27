
#include <stdint.h>
#include <openssl/md5.h>
#include <stdio.h>

const uint64_t start_clock = 11984264945;
const uint64_t finish_clock = 11989264940;

const uint32_t start_data[3] = { 0x47a1c013, 0x15809826, 0x42553b49 };
const uint32_t finish_data[3] = { 0x09bd69af, 0x2ab2c650, 0x927c94de };

void transform (uint32_t data[32])
{
    unsigned char string[24];
    
    for (int i = 0; i != 24; ++i) {
        uint32_t w = data[i / 8];
        w >>= (i % 8) * 4;
        w &= 0xf;
        string[i] = "0123456789ABCDEF"[w];
    }

    MD5_CTX c;
    MD5_Init (&c);
    MD5_Update (&c, string, 17);
    unsigned char md[16];
    MD5_Final (md, &c);
    // Convert little endian back to 32-bit words.
    data[0] = md[0] + md[1] * 256 + md[2] * 65536 + md[3] * 16777216;
    data[1] = md[4] + md[5] * 256 + md[6] * 65536 + md[7] * 16777216;
    data[2] = md[8] + md[9] * 256 + md[10] * 65536 + md[11] * 16777216;
}

int main()
{
    uint32_t data[3] = { 0x01234567, 0x78abcdef, 0xfc9639da };
    for (uint64_t i = start_clock; i != finish_clock; i += 65) {
        transform (data);
    }

    printf ("%08x %08x %08x\n", data[0], data[1], data[2]);
    return 0;
}


