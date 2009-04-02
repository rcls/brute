
#include <assert.h>
#include <openssl/md5.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "jtag-io.h"

typedef struct result_t {
    uint64_t clock;
    uint32_t data[3];
    int ram_slot;
    struct result_t * hash_next;
    const struct result_t * channel_prev;
} result_t;


void transform (const uint32_t din[3], uint32_t dout[3])
{
    unsigned char string[24];
    
    for (int i = 0; i != 24; ++i) {
        uint32_t w = din[i / 8];
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
    dout[0] = md[0] + md[1] * 256 + md[2] * 65536 + md[3] * 16777216;
    dout[1] = md[4] + md[5] * 256 + md[6] * 65536 + md[7] * 16777216;
    dout[2] = md[8] + md[9] * 256 + md[10] * 65536 + md[11] * 16777216;
}


bool match (const uint32_t A[3], const uint32_t B[3])
{
    return A[0] == B[0] && A[1] == B[1] && (A[2] & 15) == (B[2] & 15);
}


static void finish (result_t * MA, result_t * MB)
{
    printf ("HIT!!!!\n");
    printf ("%12lu %08x %08x %08x [%3lu]\n",
            MA->clock, MA->data[0], MA->data[1], MA->data[2],
            MA->clock % STAGES);
    printf ("%12lu %08x %08x %08x [%3lu]\n",
            MB->clock, MB->data[0], MB->data[1], MB->data[2],
            MB->clock % STAGES);

    result_t CA = *(MA->channel_prev);
    result_t CB = *(MB->channel_prev);

    assert (MA->clock % STAGES == MA->channel_prev->clock % STAGES);
    assert (MB->clock % STAGES == MB->channel_prev->clock % STAGES);

    uint64_t gapA = (MA->clock - MA->channel_prev->clock) / STAGES;
    uint64_t gapB = (MB->clock - MB->channel_prev->clock) / STAGES;
    uint64_t window;

    if (gapA < gapB) {
        window = gapA;
        printf ("Fast forward first by %lu\n", gapB - gapA);
    }
    else {
        window = gapB;
        printf ("Fast forward second by %lu\n", gapA - gapB);
    }
    for (uint64_t i = gapA; i < gapB; ++i)
        transform (CB.data, CB.data);
    for (uint64_t i = gapB; i < gapA; ++i)
        transform (CA.data, CA.data);

    printf ("Window size is %lu\n", window);

    for (uint64_t i = 0; i < window; ++i) {
        uint32_t NA[3];
        uint32_t NB[3];
        transform (CA.data, NA);
        transform (CB.data, NB);
        if (match (NA, NB)) {
            printf ("%08x %08x %08x -> %08x %08x %08x\n",
                    CA.data[0], CA.data[1], CA.data[2], NA[0], NA[1], NA[2]);
            printf ("%08x %08x %08x -> %08x %08x %08x\n",
                    CB.data[0], CB.data[1], CB.data[2], NB[0], NB[1], NB[2]);
            exit (EXIT_SUCCESS);
        }

        CA.data[0] = NA[0];
        CA.data[1] = NA[1];
        CA.data[2] = NA[2];
        CB.data[0] = NB[0];
        CB.data[1] = NB[1];
        CB.data[2] = NB[2];
    }
    printf ("%08x %08x %08x\n", CA.data[0], CA.data[1], CA.data[2]);
    printf ("%08x %08x %08x\n", CB.data[0], CB.data[1], CB.data[2]);
    fflush (NULL);
    abort();
}


static result_t * channel_last[STAGES];
static int channels_seeded;


static void add_result (result_t * result)
{
    static result_t * hash[256];

    int channel = result->clock % STAGES;

    if (channel_last[channel] == NULL) {
        if ((result->data[0] & 0xffffff) == 0)
            return;
        ++channels_seeded;
    }

    result_t * r = malloc (sizeof (result_t));
    *r = *result;
    r->hash_next = NULL;
    
    r->channel_prev = channel_last[channel];
    channel_last[channel] = r;

    printf ("%12lu %08x %08x %08x [%3u]%s\n",
            r->clock, r->data[0], r->data[1], r->data[2], channel,
            r->channel_prev ? "" : " - first on channel");
    if (r->channel_prev == NULL)
        return;

    // Add to the hash table.
    result_t ** bucket = &hash[r->data[1] & 255];
    // Check for 68 bits of agreement.
    for ( ; *bucket; bucket = &(*bucket)->hash_next)
        if (match (r->data, (*bucket)->data))
            finish (r, *bucket);

    *bucket = r;
}


static void seed (void)
{
    for (int i = 0; i != STAGES; ++i) {
        if (channel_last[i] != NULL)
            continue;

        printf (".");
        fflush (stdout);
        uint64_t clock = read_clock();

        clock += FREQ / 50;
        clock -= clock % STAGES;
        clock += i;
        // The clock comparison is registered, giving a one-cycle delay.
        load_md5 (clock - 1, i + 256, i + 256, i + 256);
        usleep (20003);
    }
    printf ("\n");
}


int main()
{
    open_serial();
    jtag_reset();

    setvbuf (stdout, NULL, _IOLBF, 0);

    printf ("ID Code is %08x\n", read_id());

    int index = 0;
    uint64_t clock = 0;

    while (true) {
        result_t result;
        result.ram_slot = index & 255;
        read_result (index & 255, &result.clock, result.data);
        if (result.clock > clock) {
            add_result (&result);
            clock = result.clock;
            ++index;
        }
        else if (channels_seeded < STAGES) {
            seed();
        }
        else
            sleep (1);
    }
}
