
#include <openssl/md5.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "jtag-io.h"

static void transform (uint32_t data[32])
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
    open_serial();

    printf ("ID Code is %08x\n", read_id());

    uint64_t clock1 = read_clock();
    uint64_t load_clock = clock1 + FREQ / 20;
    load_md5 (clock1 + FREQ / 20, 0x01234567, 0x78abcdef, 0xfc9639da);
    usleep (100000);

    uint64_t sample_clock = load_clock + (FREQ / 10 / STAGES) * STAGES;
    sample_md5 (sample_clock);
    usleep (100000);

    uint32_t sample[3];

    bool got_load = false;
    bool got_sample = false;

    uint64_t clock2 = read_clock();

    printf ("%lu to %lu [%lu iterations]\n", clock1, clock2, clock2 - clock1);

    for (int i = 0; i != 256; ++i) {
        uint64_t clock;
        uint32_t data[3];
        read_result (i, &clock, data);
        printf ("%12lu %08x %08x %08x [%lu]%s\n",
                clock, data[0], data[1], data[2], clock % 65,
                (clock1 <= clock && clock <= clock2) ? " *" : "");
        if (clock == load_clock)
            got_load = true;

        if (clock == sample_clock) {
            got_sample = true;
            memcpy (sample, data, sizeof (data));
        }
    }

    if (!got_load)
        printf_exit ("Failed to load data\n");
    if (!got_sample)
        printf_exit ("Failed to sample data\n");

    uint32_t data[3] = { 0x01234567, 0x78abcdef, 0xfc9639da };
    for (uint64_t i = load_clock; i != sample_clock; i += STAGES) {
        transform (data);
    }

    printf ("Calced: %08x %08x %08x\n", data[0], data[1], data[2]);
    printf ("Sample: %08x %08x %08x\n", sample[0], sample[1], sample[2]);

    if (memcmp (data, sample, sizeof (data)) != 0)
        printf_exit ("Mismatch\n");

    jtag_reset();

    return 0;
}
