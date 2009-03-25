
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include <assert.h>
#include <openssl/md5.h>

// The two jtag commands
enum {
    USER1 = 2,
    USER2 = 3,
};
// The bytes we send to USER1 as opcodes.
enum {
    op_read_result = 1,         // 8 opcode, 8 clock, returns 48 clock, 96 data.
    op_load_md5 = 2,                    // 8 opcode, 48 clock, 96 data.
    op_sample_md5 = 3,                  // 8 opcode, 48 clock.
    op_read_clock = 4,                  // 8 clock, returns 48 data.
};


// JTAG port bits.
enum {
    MASK_TDI = 1,
    MASK_TCK = 2,
    MASK_TMS = 4,
};


typedef struct result_t {
    uint64_t clock;
    uint32_t data[3];
    int ram_slot;
    struct result_t * hash_next;
    const struct result_t * channel_prev;
} result_t;


// The 'serial port' for jtag.
static int serial_port;


static void perror_exit (const char * w) __attribute__ ((noreturn));
void perror_exit (const char * w)
{
    perror (w);
    exit (EXIT_FAILURE);
}


static void printf_exit (const char * w, ...) __attribute__ ((noreturn));
void printf_exit (const char * w, ...)
{
    va_list args;
    va_start (args, w);
    vfprintf (stderr, w, args);
    va_end (args);
    exit (EXIT_FAILURE);
}

static void write_data (const unsigned char * start,
                        const unsigned char * end)
{
    while (start != end) {
        int r = write (serial_port, start, end - start);
        if (r < 0)
            perror_exit ("write");
        start += r;
    }
}


static void read_data (unsigned char * buf, size_t len)
{
    while (len > 0) {
        int r = read (serial_port, buf, len);
        if (r < 0)
            perror_exit ("read");
        if (r == 0)
            printf_exit ("EOF");
        buf += r;
        len -= r;
    }
}


static unsigned char * append_tms (unsigned char * p, bool bit)
{
    *p++ = '0' + MASK_TMS * !!bit;
    *p++ = '0' + MASK_TMS * !!bit + MASK_TCK;
    return p;
}


static unsigned char * append_nq (unsigned char * p,
                                  uint64_t data, int bits, bool last)
{
    for (int i = 1; i <= bits; ++i) {
        p[0] = '0' + (data & 1) * MASK_TDI;
        if (last && i == bits)
            p[0] |= MASK_TMS;
        p[1] = p[0] + MASK_TCK;
        p += 2;
        data >>= 1;
    }
    return p;
}


static unsigned char * append_ir (unsigned char * p, int comm)
{
    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 1);              // select-ir-scan.
    p = append_tms (p, 0);              // capture-ir.
    p = append_tms (p, 0);              // shift-ir.
    p = append_nq (p, comm, 6, true);   // ends in exit1-ir.
    p = append_tms (p, 1);              // to update-ir.
    p = append_tms (p, 0);              // to runtest-idle.

    return p;
}


static uint64_t parse_bits (unsigned char * p, int num)
{
    uint64_t result = 0;
    for (int i = num; i != 0; ) {
        --i;
        if (p[i] == 'i')
            result = result * 2 + 1;
        else if (p[i] == 'o')
            result = result * 2;
        else
            printf_exit ("Illegal recved char %02x\n", p[i]);
    }
    return result;
}


static void write_read (unsigned char * buf,
                        unsigned char * p,
                        size_t count)
{
    p = append_ir (p, USER1);

    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 0);              // capture-dr.
    p = append_tms (p, 0);              // shift-dr.

    for (int i = 1; i <= count; ++i) {
        p[0] = '0';
        if (i == count)
            p[0] |= MASK_TMS;
        p[1] = '\n';
        p[2] = p[0] + MASK_TCK;
        p += 3;
    }

    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_data (buf, p);
    read_data (buf, count);
}


static void load_md5 (uint64_t clock,
                      uint32_t load0, uint32_t load1, uint32_t load2)
{
    unsigned char obuf[2048];
    unsigned char * p = obuf;

    p = append_ir (p, USER1);

    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 0);              // capture-dr.
    p = append_tms (p, 0);              // shift-dr.

    // Send the 3 words.
    p = append_nq (p, load0, 32, false);
    p = append_nq (p, load1, 32, false);
    p = append_nq (p, load2, 32, false);
    // The clock.
    p = append_nq (p, clock, 48, false);

    p = append_nq (p, op_load_md5, 8, true); // ends in exit1-dr.
    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_data (obuf, p);
}


static void sample_md5 (uint64_t clock)
{
    unsigned char obuf[2048];
    unsigned char * p = obuf;

    p = append_ir (p, USER1);

    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 0);              // capture-dr.
    p = append_tms (p, 0);              // shift-dr.

    // The clock.
    p = append_nq (p, clock, 48, false);

    p = append_nq (p, op_sample_md5, 8, true); // ends in exit1-dr.
    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_data (obuf, p);
}


static void read_result (int location, uint64_t * clock, uint32_t data[3])
{
    unsigned char obuf[2048];
    unsigned char * p = obuf;

    p = append_ir (p, USER1);

    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 0);              // capture-dr.
    p = append_tms (p, 0);              // shift-dr.

    p = append_nq (p, location, 8, false); // location.
    p = append_nq (p, op_read_result, 8, true); // ends in exit1-dr.
    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_read (obuf, p, 144);

    data[0] = parse_bits (obuf, 32);
    data[1] = parse_bits (obuf + 32, 32);
    data[2] = parse_bits (obuf + 64, 32);
    *clock = parse_bits (obuf + 96, 48);
}


static uint64_t read_clock (void)
{
    unsigned char obuf[2048];
    unsigned char * p = obuf;

    p = append_ir (p, USER1);

    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 0);              // capture-dr.
    p = append_tms (p, 0);              // shift-dr.

    p = append_nq (p, op_read_clock, 8, true); // ends in exit1-dr.
    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    //write_read (obuf, p, 48);
    write_read (obuf, p, 144);

    return parse_bits (obuf + 96, 48);
}


static uint32_t read_id (void)
{
    unsigned char obuf[2048];
    unsigned char * p = obuf;

    p = append_ir (p, 9);
    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 0);              // capture-dr.
    p = append_tms (p, 0);              // shift-dr.

    for (int i = 1; i <= 32; ++i) {
        p[0] = '0';
        if (i == 32)
            p[0] |= MASK_TMS;
        p[1] = '\n';
        p[2] = p[0] + MASK_TCK;
        p += 3;
    }

    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_data (obuf, p);
    read_data (obuf, 32);

    return parse_bits (obuf, 32);
}


static void jtag_reset (void)
{
    unsigned char obuf[100];
    unsigned char * p = obuf;
    for (int i = 0; i != 6; ++i)
        p = append_tms (p, 1);
    p = append_tms (p, 0);              // runtest - idle.
    write_data (obuf, p);
}


static void open_serial (void)
{
    serial_port = open ("/dev/ttyUSB0", O_RDWR);
    if (serial_port < 0)
        perror_exit ("open /dev/ttyUSB0");

    struct termios options;

    if (tcgetattr (serial_port, &options) < 0)
        perror_exit ("tcgetattr failed");

    cfmakeraw (&options);

    options.c_cflag |= CREAD | CLOCAL;

    //options.c_lflag &= ~(/* ISIG*/ /* | TOSTOP*/ | FLUSHO);
    //options.c_lflag |= NOFLSH;

    options.c_iflag &= ~(IXOFF | ISTRIP | IMAXBEL);
    options.c_iflag |= BRKINT;

    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;

    if (tcsetattr (serial_port, TCSANOW, &options) < 0)
        perror_exit ("tcsetattr failed");

    if (tcflush (serial_port, TCIOFLUSH) < 0)
        perror_exit ("tcflush failed");
}

void yeah(void)
{
    load_md5 (0,0,0,0);
/*     uint64_t clock; */
/*     uint32_t data[3]; */
/*     read_result (0, &clock, data); */
/*     sample_md5 (0); */
}

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
    return A[0] == B[0] && B[1] == B[1] && (A[2] & 255) == (B[2] & 255);
}


static void finish (result_t * MA, result_t * MB)
{
    printf ("HIT!!!!\n");
    printf ("%12lu %08x %08x %08x [%2lu]\n",
            MA->clock, MA->data[0], MA->data[1], MA->data[2], MA->clock % 65);
    printf ("%12lu %08x %08x %08x [%2lu]\n",
            MB->clock, MB->data[0], MB->data[1], MB->data[2], MB->clock % 65);

    result_t CA = *(MA->channel_prev);
    result_t CB = *(MB->channel_prev);

    assert (MA->clock % 65 == MA->channel_prev->clock % 65);
    assert (MB->clock % 65 == MB->channel_prev->clock % 65);

    uint64_t gapA = (MA->clock - MA->channel_prev->clock) / 65;
    uint64_t gapB = (MB->clock - MB->channel_prev->clock) / 65;
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


static void add_result (result_t * result)
{
    static result_t * channel_last[65];

    static result_t * hash[256];

    result_t * r = malloc (sizeof (result_t));
    *r = *result;
    r->hash_next = NULL;
    
    int channel = r->clock % 65;
    r->channel_prev = channel_last[channel];
    channel_last[channel] = r;

    printf ("%12lu %08x %08x %08x [%2u]%s\n",
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


static int compare_result (const void * aa, const void * bb)
{
    const result_t * a = aa;
    const result_t * b = bb;
    if (a->clock < b->clock)
        return -1;
    if (a->clock > b->clock)
        return 1;
    return 0;
}


int main()
{
    open_serial();
    jtag_reset();

    printf ("ID Code is %08x\n", read_id());

    // Start by sampling all 65 channels.
    for (int i = 0; i != 65; ++i) {
        printf (".");
        fflush (stdout);
        // Set a target about 25ms in the future.
        uint64_t clock = read_clock();
        uint64_t target = clock + 25 * 50000;
        target = target - target % 65 + i;
        sample_md5 (target);
        usleep (50000);
    }
    printf ("\n");

    result_t first_results[256];
    result_t * p = first_results;

    // Now start collating data.  Firstly, scan all 256 results, and just keep
    // the non-zero ones.
    for (int i = 0; i != 256; ++i) {
        printf (".");
        fflush (stdout);
        p->ram_slot = i;
        read_result (i, &p->clock, p->data);
        if (p->clock != 0)
            ++p;
    }
    printf ("\n");

    qsort (first_results, p - first_results, sizeof (result_t), compare_result);
    for (result_t * q = first_results; q != p; ++q)
        add_result (q);

    int index = p[-1].ram_slot + 1;
    uint64_t clock = p[-1].clock;
    while (true) {
        result_t result;
        result.ram_slot = index & 255;
        read_result (index & 255, &result.clock, result.data);
        if (result.clock <= clock)
            sleep (1);
        else {
            add_result (&result);
            clock = result.clock;
            ++index;
        }
    }
}
