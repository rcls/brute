
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "jtag-io.h"


// JTAG port bits.
enum {
    MASK_TDI = 1,
    MASK_TCK = 2,
    MASK_TMS = 4,
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


void load_md5 (uint64_t clock, uint32_t load0, uint32_t load1, uint32_t load2)
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

    p = append_nq (p, op_load_md5, 8, true); // ends in exit1-dr.
    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_data (obuf, p);
}


void sample_md5 (uint64_t clock)
{
    unsigned char obuf[2048];
    unsigned char * p = obuf;

    p = append_ir (p, USER1);

    p = append_tms (p, 1);              // to select-dr-scan.
    p = append_tms (p, 0);              // capture-dr.
    p = append_tms (p, 0);              // shift-dr.

    // The clock.
    p = append_nq (p, clock - MATCH_DELAY, 48, false);

    p = append_nq (p, op_sample_md5, 8, true); // ends in exit1-dr.
    p = append_tms (p, 1);                   // update-ir.
    p = append_tms (p, 0);                   // runtest-idle.

    write_data (obuf, p);
}


void read_result (int location, uint64_t * clock, uint32_t data[3])
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


uint64_t read_clock (void)
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


uint32_t read_id (void)
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
