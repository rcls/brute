#ifndef JTAG_IO_H
#define JTAG_IO_H

#include <stdint.h>


#define STAGES 195
#define PIPELINES 2
#define FREQ (150 * 1000 * 1000)
#define MATCH_DELAY 3

#define BITS 96
#define NIBBLES ((BITS + 3) / 4)
#define LAST_NIBBLE_MASK (15 >> (3 & -BITS))

#define TRIGGER_BITS 30
#define TRIGGER_MASK (TRIGGER_BITS == 32 ? 0xffffffff : (1 << TRIGGER_BITS) - 1)

// The two jtag commands
enum {
    USER1 = 2,
    USER2 = 3,
};


void perror_exit (const char * w) __attribute__ ((noreturn));
void printf_exit (const char * w, ...) __attribute__ ((noreturn));

void load_md5 (int pipeline,
               uint64_t clock, uint32_t load0, uint32_t load1, uint32_t load2);
void sample_md5 (int pipeline, uint64_t clock);
void read_result (int pipeline,
                  int location, uint64_t * clock, uint32_t data[3]);
uint64_t read_clock (void);
uint32_t read_id (void);
void jtag_reset (void);
void open_serial (void);

void external_clock (uint64_t c);
uint64_t start_clock (void);

// Nothing to do with jtag...
void transform (const uint32_t din[3], uint32_t dout[3]);

#endif
