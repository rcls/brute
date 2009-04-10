
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <openssl/md5.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "jtag-io.h"

#define WIPE "\e[J"

static FILE * datafile;

typedef struct result_t {
    uint64_t clock;
    uint32_t data[3];
    int pipe;
    struct result_t * hash_next;
    const struct result_t * channel_prev;
} result_t;

// R clock pipe w1 w2 w3 [result from pipeline]
// H clock pipe clock pipe delta a1 a2 a3 b1 b2 b3 c1 c2 c3
//       [hit data from previous results]
// E clock pipe clock pipe a1 a2 a3 b1 b2 b3 [failure from previous result]


// The bit masks for hit comparisons.
static const uint32_t mask0 = BITS >= 32 ? 0xfffffff : (1 << BITS) - 1;
static const uint32_t mask1 = BITS >= 32
    ? BITS >= 64 ? 0xfffffff : (1 << (BITS - 32)) - 1 : 0;
static const uint32_t mask2 = BITS >= 64 ? (1 << (BITS - 64)) - 1 : 0;


// The last result recorded on each channel.
static result_t * channel_last[STAGES * PIPELINES];
static int channels_seeded;

// Hash table of results.
#define HASH_SIZE (1 << (BITS / 2 - TRIGGER_BITS))
static result_t * hash[HASH_SIZE];

// Number of recorded checksums.
static uint64_t cycle_count;
static int result_count;


static bool match (const uint32_t A[3], const uint32_t B[3])
{
    return (A[0] & mask0) == (B[0] & mask0)
        && (A[1] & mask1) == (B[1] & mask1)
        && (A[2] & mask2) == (B[2] & mask2);
}


static void finish_sync (result_t * MA, result_t * MB)
{
    printf ("\nHIT!!!!\n");
    printf ("%12lu %08x %08x %08x %c[%3lu]\n",
            MA->clock, MA->data[0], MA->data[1], MA->data[2],
            'A' + MA->pipe, MA->clock % STAGES);
    printf ("%12lu %08x %08x %08x %c[%3lu]\n",
            MB->clock, MB->data[0], MB->data[1], MB->data[2],
            'A' + MB->pipe, MB->clock % STAGES);

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
            fprintf (datafile, "H %lu %u %lu %u %lu %08x %08x %08x "
                     "%08x %08x %08x %08x %08x %08x %08x %08x %08x\n",
                     MA->clock, MA->pipe, MB->clock, MB->pipe, window - i,
                     CA.data[0], CA.data[1], CA.data[2], NA[0], NA[1], NA[2],
                     CB.data[0], CB.data[1], CB.data[2], NB[0], NB[1], NB[2]);

            printf ("\rHit %lu[%lu]%c %lu[%lu]%c, delta %lu " WIPE "\n"
                    "%08x %08x %08x -> %08x %08x %08x\n"
                    "%08x %08x %08x -> %08x %08x %08x\n",
            CA.clock, CA.clock % STAGES, 'A' + CA.pipe,
            CB.clock, CB.clock % STAGES, 'A' + CB.pipe, window - i,
            CA.data[0], CA.data[1], CA.data[2], NA[0], NA[1], NA[2],
            CB.data[0], CB.data[1], CB.data[2], NB[0], NB[1], NB[2]);
            fflush (NULL);
            return;
        }

        CA.data[0] = NA[0];
        CA.data[1] = NA[1];
        CA.data[2] = NA[2];
        CB.data[0] = NB[0];
        CB.data[1] = NB[1];
        CB.data[2] = NB[2];
    }
    fprintf (datafile, "E %lu %u %lu %u %08x %08x %08x %08x %08x %08x\n",
             MA->clock, MA->pipe, MB->clock, MB->pipe,
             CA.data[0], CA.data[1], CA.data[2],
             CB.data[0], CB.data[1], CB.data[2]);
    printf ("%08x %08x %08x\n", CA.data[0], CA.data[1], CA.data[2]);
    printf ("%08x %08x %08x\n", CB.data[0], CB.data[1], CB.data[2]);
    fflush (NULL);
    // In theory these writes are not thread-safe but I don't care.
    if (CA.data[0] != MA->data[0] || CA.data[1] != MA->data[1]
        || CA.data[2] != MA->data[2]) {
        printf ("Kill %12lu %c[%3lu]\n", MA->clock, 'A' + MA->pipe,
                MA->clock % STAGES);
        MA->data[0] = 0xfffffff;
        MA->data[1] = 0xfffffff;
        MA->data[2] = 0xfffffff;
    }
    if (CB.data[0] != MB->data[0] || CB.data[1] != MB->data[1]
        || CB.data[2] != MB->data[2]) {
        printf ("Kill %12lu %c[%3lu]\n", MB->clock, 'B' + MB->pipe,
                MB->clock % STAGES);
        MB->data[0] = 0xfffffff;
        MB->data[1] = 0xfffffff;
        MB->data[2] = 0xfffffff;
    }
}


static void * finish_thread_func (void * p)
{
    result_t ** items = p;
    result_t * A = items[1];
    result_t * B = items[2];
    free (items);
    finish_sync (A, B);
    return NULL;
}


static void finish_async (result_t * A, result_t * B)
{
    printf ("\n");
    result_t ** items = malloc (2 * sizeof (result_t *));
    if (items == NULL)
        printf_exit ("Out of memory processing hit\n");
    items[0] = A;
    items[1] = B;
    pthread_t t;
    int r = pthread_create (&t, NULL, finish_thread_func, items);
    if (r != 0)
        printf_exit ("Failed to create thread: %s\n", strerror (r));
    pthread_detach (t);
}


static void add_result (result_t * result, bool for_real)
{

    int channel = result->clock % STAGES;
    channel = channel * PIPELINES + result->pipe;

    if (channel_last[channel] == NULL) {
        if ((result->data[0] & TRIGGER_MASK) == 0)
            return;        // Ignore pre-seed data.
        ++channels_seeded;
    }

    result_t * r = malloc (sizeof (result_t));
    *r = *result;
    r->hash_next = NULL;

    if (r->data[0] & TRIGGER_MASK)
        r->channel_prev = NULL;
    else {
        r->channel_prev = channel_last[channel];
        cycle_count += (r->clock - r->channel_prev->clock) / STAGES;
        ++result_count;
    }
    channel_last[channel] = r;

    if (for_real)
        fprintf (datafile, "R %lu %u %x %x %x\n",
                 r->clock, r->pipe, r->data[0], r->data[1], r->data[2]);

    // Calculate approximate done percentage.
    double total = 1ul << (BITS / 2);
    if (BITS & 1)
        total *= M_SQRT2;

    printf ("\r%g%% %u %12lu %08x %08x %08x %c[%3u]%s" WIPE,
            100 * cycle_count / total, result_count,
            r->clock, r->data[0], r->data[1], r->data[2],
            'A' + r->pipe, channel / PIPELINES,
            r->channel_prev ? "" : " init");

    fflush (NULL);

    if (r->channel_prev == NULL)
        return;

    // Add to the hash table.
    result_t ** bucket = &hash[r->data[1] % HASH_SIZE];
    // Check for all bits of agreement.
    for ( ; *bucket; bucket = &(*bucket)->hash_next)
        if (for_real && match (r->data, (*bucket)->data))
            finish_async (r, *bucket);

    *bucket = r;
}


static void read_log_result (void)
{
    // FIXME - record in log chain.
    result_t r;
    if (fscanf (datafile, " %lu %u %x %x %x ",
                &r.clock, &r.pipe, &r.data[0], &r.data[1], &r.data[2])
        != 5) {
        fprintf (stderr, "Bogus R line in log file\n");
        return;
    }

    external_clock (r.clock);
    add_result (&r, false);
}


typedef struct logged_hit_t {
    uint64_t clockA;
    uint64_t clockB;
    int pipeA;
    int pipeB;
} logged_hit_t;


static logged_hit_t * logged_hit_list;
static size_t logged_hit_count;


static void add_logged_hit (const result_t * A, const result_t * B)
{
    if (A->clock > B->clock || (A->clock == B->clock && A->pipe > B->pipe)) {
        const result_t * tmp = A;
        A = B;
        B = tmp;
    }

    logged_hit_list = realloc (logged_hit_list,
                             sizeof (logged_hit_t) * (logged_hit_count + 1));
    if (logged_hit_list == NULL)
        printf_exit ("Out of memory with %zu read hits\n", logged_hit_count);
    logged_hit_list[logged_hit_count].clockA = A->clock;
    logged_hit_list[logged_hit_count].pipeA  = A->pipe;
    logged_hit_list[logged_hit_count].clockB = B->clock;
    logged_hit_list[logged_hit_count].pipeB  = B->pipe;
    ++logged_hit_count;
}


static void read_log_hit (void)
{
    result_t CA;
    result_t CB;
    uint64_t offset;
    uint32_t NA[3];
    uint32_t NB[3];

#define H3 "%x %x %x "
    if (fscanf (datafile, " %lu %u %lu %u %lu " H3 H3 H3 H3,
                &CA.clock, &CA.pipe, &CB.clock, &CB.pipe, &offset,
                &CA.data[0], &CA.data[1], &CA.data[2], &NA[0], &NA[1], &NA[2],
                &CB.data[0], &CB.data[1], &CB.data[2], &NB[0], &NB[1], &NB[2])
        != 17) {
        fprintf (stderr, "Bogus H line in log file\n");
        return;
    }

    printf ("\nRead hit %lu[%lu]%c %lu[%lu]%c, delta %lu\n"
            "%08x %08x %08x -> %08x %08x %08x\n"
            "%08x %08x %08x -> %08x %08x %08x\n",
            CA.clock, CA.clock % STAGES, 'A' + CA.pipe,
            CB.clock, CB.clock % STAGES, 'A' + CB.pipe, offset,
            CA.data[0], CA.data[1], CA.data[2], NA[0], NA[1], NA[2],
            CB.data[0], CB.data[1], CB.data[2], NB[0], NB[1], NB[2]);
    add_logged_hit (&CA, &CB);
}


static void read_log_error (void)
{
    result_t CA;
    result_t CB;

    if (fscanf (datafile, " %lu %u %lu %u %x %x %x %x %x %x ",
                &CA.clock, &CA.pipe, &CB.clock, &CB.pipe,
                &CA.data[0], &CA.data[1], &CA.data[2],
                &CB.data[0], &CB.data[1], &CB.data[2]) != 10) {
        fprintf (stderr, "Bogus E line in log file\n");
        return;
    }

    printf ("Read hit error %lu[%lu]%c %lu[%lu]%c\n",
            CA.clock, CA.clock % STAGES, 'A' + CA.pipe,
            CB.clock, CB.clock % STAGES, 'A' + CB.pipe);

    add_logged_hit (&CA, &CB);
    // FIXME - kill the error entries.
}


static void read_log_file (void)
{
    while (true) {
        int c = fgetc (datafile);
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
        case EOF:
            return;
        default:
            fprintf (stderr, "Ignoring bogus char %02x in log file", c);
            break;
        }
    }
}


static int logged_hit_compare (const void * XX, const void * YY)
{
    const logged_hit_t * X = XX;
    const logged_hit_t * Y = YY;
    if (X->clockA != Y->clockA)
        return X->clockA < Y->clockA ? -1 : 1;
    if (X->clockB != Y->clockB)
        return X->clockB < Y->clockB ? -1 : 1;
    if (X->pipeA != Y->pipeA)
        return X->pipeA < Y->pipeA ? -1 : 1;
    if (X->pipeB != Y->pipeB)
        return X->pipeB < Y->pipeB ? -1 : 1;
    return 0;
}


static void catch_up_hit (result_t * A, result_t * B)
{
    if (A->clock > B->clock || (A->clock == B->clock && A->pipe > B->pipe)) {
        result_t * tmp = A;
        A = B;
        B = tmp;
    }

    logged_hit_t hit;
    hit.clockA = A->clock;
    hit.clockB = B->clock;
    hit.pipeA  = A->pipe;
    hit.pipeB  = B->pipe;

    // Do a binary search to see if we've already logged the details.
    int low = -1;
    int high = logged_hit_count;
    while (high - low > 1) {
        int mid = (high + low) >> 1;
        int c = logged_hit_compare (&logged_hit_list[mid], &hit);
        if (c == 0)
            return;
        if (c < 0)
            low = mid;
        else
            high = mid;
    }

    // Not found, record it.
    finish_sync (A, B);
}


static void catch_up_hits (void)
{
    qsort (logged_hit_list, logged_hit_count, sizeof (logged_hit_t),
           logged_hit_compare);

    for (int i = 0; i != HASH_SIZE; ++i)
        // Chains should be short enough that the quadratic search doesn't
        // matter.
        for (result_t * A = hash[i]; A != NULL; A = A->hash_next)
            for (result_t * B = A->hash_next; B != NULL; B = B->hash_next)
                if (match (A->data, B->data))
                    catch_up_hit (A, B);
}


static void seed (void)
{
    int done = 0;
    uint32_t tt = time (NULL);
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

        load_md5 (pipe, clock, tt, i, (uint32_t) clock);
        usleep (20000);

        ++done;
    }
}


int main (int argc, const char * const argv[])
{
    if (argc < 2 || argv[1][0] == '-')
        printf_exit ("Usage: %s <log file>\n", argv[0]);

    datafile = fopen (argv[1], "a+");
    if (datafile == NULL)
        printf_exit ("open %s: %s\n", argv[1], strerror (errno));

    read_log_file();
    catch_up_hits();

    open_serial();
    jtag_reset();

    // Wipe out channel lasts, just to make sure we don't create false
    // channel_prev links.
    memset (channel_last, 0, sizeof (channel_last));
    channels_seeded = 0;

    // Line buffer all output.
    setvbuf (stdout, NULL, _IOLBF, 0);
    setvbuf (datafile, NULL, _IOLBF, 0);

    printf ("ID Code is %08x\n", read_id());

    int index[PIPELINES];
    uint64_t clock[PIPELINES];

    uint64_t iclk = start_clock();

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
                result.pipe = pipe;
                clock[pipe] = result.clock;
                if (result.clock > iclk)
                    add_result (&result, true);
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
