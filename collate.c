
#include <assert.h>
#include <openssl/md5.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "jtag-io.h"

typedef struct result_t {
    uint64_t clock;
    uint32_t data[3];
    int ram_slot;
    struct result_t * hash_next;
    const struct result_t * channel_prev;
} result_t;


const uint32_t mask0 = BITS >= 32 ? 0xfffffff : (1 << BITS) - 1;
const uint32_t mask1 = BITS >= 32
    ? BITS >= 64 ? 0xfffffff : (1 << (BITS - 32)) - 1
    : 0;
const uint32_t mask2 = BITS >= 64 ? (1 << (BITS - 64)) - 1 : 0;

static bool match (const uint32_t A[3], const uint32_t B[3])
{
    return (A[0] & mask0) == (B[0] & mask0)
        && (A[1] & mask1) == (B[1] & mask1)
        && (A[2] & mask2) == (B[2] & mask2);
}


static void finish (result_t * MA, result_t * MB)
{
    printf ("HIT!!!!\n");
    printf ("%12lu %08x %08x %08x %c[%3lu]\n",
            MA->clock, MA->data[0], MA->data[1], MA->data[2],
            'A' + MA->ram_slot % PIPELINES, MA->clock % STAGES);
    printf ("%12lu %08x %08x %08x %c[%3lu]\n",
            MB->clock, MB->data[0], MB->data[1], MB->data[2],
            'A' + MB->ram_slot % PIPELINES, MB->clock % STAGES);

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


static result_t * channel_last[STAGES * PIPELINES];
static int channels_seeded;


static void add_result (result_t * result)
{
#define HASH_SIZE (1 << (BITS / 2 - TRIGGER_BITS))
    static result_t * hash[HASH_SIZE];

    int channel = result->clock % STAGES;
    channel = channel * PIPELINES + result->ram_slot % PIPELINES;

    if (channel_last[channel] == NULL) {
        if ((result->data[0] & TRIGGER_MASK) == 0)
            return;        // Ignore pre-seed data.
        ++channels_seeded;
    }

    result_t * r = malloc (sizeof (result_t));
    *r = *result;
    r->hash_next = NULL;

    r->channel_prev = channel_last[channel];
    channel_last[channel] = r;

    printf ("%12lu %08x %08x %08x %c[%3u]%s\n",
            r->clock, r->data[0], r->data[1], r->data[2],
            'A' + channel % PIPELINES, channel / PIPELINES,
            r->channel_prev ? "" : " - first on channel");
    if (r->channel_prev == NULL)
        return;

    // Add to the hash table.
    result_t ** bucket = &hash[r->data[1] % HASH_SIZE];
    // Check for 68 bits of agreement.
    for ( ; *bucket; bucket = &(*bucket)->hash_next)
        if (match (r->data, (*bucket)->data))
            finish (r, *bucket);

    *bucket = r;
}


static void seed (void)
{
    int done = 0;
    for (int i = 0; i != STAGES * PIPELINES && done < 10; ++i) {
        if (channel_last[i] != NULL)
            continue;

        printf (".");
        fflush (stdout);
        uint64_t clock = read_clock();
        int pipe = i % PIPELINES;
        int stage = i / PIPELINES;

        clock += FREQ / 50;
        clock -= clock % STAGES;
        clock += stage;

        load_md5 (pipe, clock, i + 256, i + 256, i + 256);
        usleep (20000);

        ++done;
    }
    printf ("\n");
}


int main()
{
    open_serial();
    jtag_reset();

    setvbuf (stdout, NULL, _IOLBF, 0);

    printf ("ID Code is %08x\n", read_id());

    int index[PIPELINES];
    uint64_t clock[PIPELINES];

    uint64_t iclk = read_clock();

    for (int i = 0; i != PIPELINES; ++i) {
        index[i] = 1;
        clock[i] = 0;
    }

    while (true) {
        bool got = false;
        for (int pipe = 0; pipe != PIPELINES; ++pipe)
            while (true) {
                result_t result;
                read_result (pipe, index[pipe] & 255,
                             &result.clock, result.data);
                if (result.clock <= clock[pipe])
                    break;

                got = true;
                result.ram_slot = (index[pipe] & 255) * PIPELINES + pipe;
                clock[pipe] = result.clock;
                if (result.clock > iclk)
                    add_result (&result);
                ++index[pipe];
            }
        if (got)
            continue;
        if (channels_seeded < STAGES * PIPELINES)
            seed();
        else
            sleep (1);
    }
}
