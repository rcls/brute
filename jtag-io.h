#ifndef JTAG_IO_H
#define JTAG_IO_H

#include <stdint.h>


#define STAGES 195
#define FREQ (150 * 1000 * 1000)
#define MATCH_DELAY 2


// The two jtag commands
enum {
    USER1 = 2,
    USER2 = 3,
};

// The bytes we send to USER1 as opcodes.
enum {
    op_read_result = 1,         // 8 opcode, 8 clock, returns 48 clock, 96 data.
    op_load_md5 = 6,                    // 8 opcode, 48 clock, 96 data.
    op_sample_md5 = 4,                  // 8 opcode, 48 clock.
    op_read_clock = 0,                  // 8 clock, returns 48 data.
};


void perror_exit (const char * w) __attribute__ ((noreturn));
void printf_exit (const char * w, ...) __attribute__ ((noreturn));

void load_md5 (uint64_t clock, uint32_t load0, uint32_t load1, uint32_t load2);
void sample_md5 (uint64_t clock);
void read_result (int location, uint64_t * clock, uint32_t data[3]);
uint64_t read_clock (void);
uint32_t read_id (void);
void jtag_reset (void);
void open_serial (void);

#endif
