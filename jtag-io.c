
#include <fcntl.h>
#include <openssl/md5.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "jtag-io.h"


// The bytes we send to USER1 as opcodes.
enum {
    opA_read_result = 1,        // 8 opcode, 8 clock, returns 48 clock, 96 data.
    opA_load_md5 = 6,           // 8 opcode, 48 clock, 96 data.
    opA_sample_md5 = 4,         // 8 opcode, 48 clock.

    opB_read_result = 1 << 4,   // 8 opcode, 8 clock, returns 48 clock, 96 data.
    opB_load_md5 = 6 << 4,      // 8 opcode, 48 clock, 96 data.
    opB_sample_md5 = 4 << 4,    // 8 opcode, 48 clock.

    op_read_clock = 0,                  // 8 clock, returns 48 data.
};


// JTAG port bits.
enum {
    MASK_TDI = 1,
    MASK_TMS = 2,
    MASK_SAMPLE = 4,
};


// The 'serial port' for jtag.
static int serial_port;


void perror_exit (const char * w)
{
    perror (w);
    exit (EXIT_FAILURE);
}


void printf_exit (const char * w, ...)
{
    va_list args;
    va_start (args, w);
    vfprintf (stderr, w, args);
    va_end (args);
    exit (EXIT_FAILURE);
}


// Reading in a log file, we track the read-in clock values, so we can adjust
// the current clock to be monotonic.
static uint64_t clock_max_external;
// The last adjusted clock value.
static uint64_t clock_last;

// Adjust a clock value by setting the top 16 bits to be close to clock_last,
// while preserving the bottom 48 bits.
static uint64_t adjust_clock (uint64_t c)
{
    static const uint64_t mask48 = (1ul << 48) - 1;
    // Apply the top bits of clock_last to c.
    c = (c & mask48) | (clock_last & ~mask48);

    // We select whichever of c, c +/- (1<<48) is closest to clock_last.  We
    // error if this jumps by more than 1<<32.
    uint64_t c_minus = c - (1ul << 48);
    uint64_t c_plus = c + (1ul << 48);

    uint64_t o = labs (c - clock_last);
    uint64_t o_minus = labs (c_minus - clock_last);
    uint64_t o_plus = labs (c_plus - clock_last);

    if (o >= (1ul << 46) && o_minus >= (1ul << 32) && o_plus >= (1ul << 46))
        printf_exit ("Clock jumps by too much (%lu -> %lu)\n",
                     clock_last & mask48, c & mask48);

    if (o_plus <= o) {
        c = c_plus;
        printf ("\nClock wraps [now at %lu 48-bit wraps]\n", c >> 48);
    }
    else if (o_minus <= o) {
        c = c_minus;
        printf ("\nClock slips back [now at %lu 48-bit wraps]\n", c >> 48);
    }

    clock_last = c;;
    return c;
}


void external_clock (uint64_t c)
{
    if (c > clock_max_external)
        clock_max_external = c;
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


static inline unsigned char * append_tms (unsigned char * p, bool bit)
{
    *p++ = '0' + MASK_TMS * !!bit;
    return p;
}


static unsigned char * append_nq (unsigned char * p,
                                  uint64_t data, int bits, bool last)
{
    for (int i = 1; i <= bits; ++i) {
        p[0] = '0' + (data & 1) * MASK_TDI;
        if (last && i == bits)
            p[0] |= MASK_TMS;
        ++p;
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
        if (p[i] == '!')
            result = result * 2 + 1;
        else if (p[i] == ' ')
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
        p[0] = '0' + MASK_SAMPLE;
        if (i == count)
            p[0] |= MASK_TMS;
        ++p;
    }

    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_data (buf, p);
    read_data (buf, count);
}


void load_md5 (int pipeline,
               uint64_t clock, uint32_t load0, uint32_t load1, uint32_t load2)
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
    p = append_nq (p, clock - MATCH_DELAY, 48, false);

    p = append_nq (p, pipeline == 0 ? opA_load_md5 : opB_load_md5,
                   8, true); // ends in exit1-dr.
    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_data (obuf, p);
}


void sample_md5 (int pipeline, uint64_t clock)
{
    unsigned char obuf[2048];
    unsigned char * p = obuf;

    p = append_ir (p, USER1);

    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 0);              // capture-dr.
    p = append_tms (p, 0);              // shift-dr.

    // The clock.
    p = append_nq (p, clock - MATCH_DELAY, 48, false);

    p = append_nq (p, pipeline == 0 ? opA_sample_md5 : opB_sample_md5,
                   8, true); // ends in exit1-dr.
    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_data (obuf, p);
}


void read_result (int pipeline,
                  int location, uint64_t * clock, uint32_t data[3])
{
    unsigned char obuf[2048];
    unsigned char * p = obuf;

    p = append_ir (p, USER1);

    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 0);              // capture-dr.
    p = append_tms (p, 0);              // shift-dr.

    p = append_nq (p, location, 8, false); // location.
    p = append_nq (p, pipeline == 0 ? opA_read_result : opB_read_result,
                   8, true); // ends in exit1-dr.
    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_read (obuf, p, 144);

    data[0] = parse_bits (obuf, 32);
    data[1] = parse_bits (obuf + 32, 32);
    data[2] = parse_bits (obuf + 64, 32);
    *clock = adjust_clock (parse_bits (obuf + 96, 48));
}


static uint64_t read_clock_raw (void)
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

    write_read (obuf, p, 48);

    return parse_bits (obuf, 48);
}


uint64_t read_clock (void)
{
    return adjust_clock (read_clock_raw());
}


uint64_t start_clock (void)
{
    uint64_t c = read_clock_raw();
    c += clock_max_external & (-1ul << 48);
    if (c <= clock_max_external)
        c += 1ul << 48;

    printf ("Initial clock at %lu wraps\n", c >> 48);
    clock_last = c;
    return c;
}


uint32_t read_id (void)
{
    unsigned char obuf[2048];
    unsigned char * p = obuf;

    p = append_ir (p, 9);
    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 0);              // capture-dr.
    p = append_tms (p, 0);              // shift-dr.

    for (int i = 1; i <= 32; ++i) {
        p[0] = '0' + MASK_SAMPLE;
        if (i == 32)
            p[0] |= MASK_TMS;
        ++p;
    }

    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_data (obuf, p);
    read_data (obuf, 32);

    return parse_bits (obuf, 32);
}


void jtag_reset (void)
{
    unsigned char obuf[100];
    unsigned char * p = obuf;
    for (int i = 0; i != 6; ++i)
        p = append_tms (p, 1);
    p = append_tms (p, 0);              // runtest - idle.
    write_data (obuf, p);
}


void open_serial (void)
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

    jtag_reset();
}


void transform (const uint32_t din[3], uint32_t dout[3])
{
    unsigned char string[NIBBLES];

    const unsigned int LAST = NIBBLES - 1;
    for (unsigned i = 0; i != LAST; ++i)
        string[i] = "0123456789abcdef"[
            (din[i / 8] >> ((i % 8) * 4)) & 15];
    string[LAST] = "0123456789abcdef"[
        (din[LAST / 8] >> ((LAST % 8) * 4)) & LAST_NIBBLE_MASK];

    MD5_CTX c;
    MD5_Init (&c);
    MD5_Update (&c, string, NIBBLES);
    unsigned char md[16];
    MD5_Final (md, &c);
    // Convert little endian back to 32-bit words.
    dout[0] = md[0] + md[1] * 256 + md[2] * 65536 + md[3] * 16777216;
    dout[1] = md[4] + md[5] * 256 + md[6] * 65536 + md[7] * 16777216;
    dout[2] = md[8] + md[9] * 256 + md[10] * 65536 + md[11] * 16777216;
}
