
#include <assert.h>
#include <stdio.h>

#include "jtag-io.h"

#define FASTFORWARD 5361656300ul
#define WINDOW 507607410ul

// R 1792905455018270 1 0 aec6f9d1 24eef6cc
#define LOWA 1792905455018270ul
#define HIGHA 1794049961441720ul
#define GAPA (FASTFORWARD + WINDOW)

// R 1318086158704845 1 0 f3a8d619 1b8eb870
#define LOWB 1318086158704845ul
#define HIGHB 1318185142149795ul
#define GAPB WINDOW

/*

Looks like we missed several hits on channel 125:

743.43% 1955682 2092565132609089 80000000 eb936702 c19d148f B[125]
HIT!!!!
1794049961441720 80000000 eb936702 c19d148f B[125]
1318185142149795 80000000 eb936702 c19d148f B[ 75]
Fast forward second by 5361656300
744.11% 1956132 2094497289968387 40000000 163887de b11c9edc B[179]
Window size is 507607410
Hit 1792905455018270[125]B 1318086158704845[75]B, delta 507607410
00000000 f3a8d619 1b8eb870 -> e1866630 a0fa21e2 67c7793b
00000000 f3a8d619 1b8eb870 -> e1866630 a0fa21e2 67c7793b

Looking at the logs, we missed a heap of data... like the collate process hung
or something.

At 0
R 1792905455018270 1 0 aec6f9d1 24eef6cc
At 1087058820
R 1793117431488170 1 0 6772ef45 69fecf60
At 2364044618
R 1793366443718780 1 c0000000 90c207f7 57c4024d
At 2412221301
R 1793375838171965 1 0 b193386d 9a494296
At 5361656300
R 1793950977996770 1 0 f3a8d619 1b8eb870
matches
R 1318086158704845 1 0 f3a8d619 1b8eb870
At end:
R 1794049961441720 1 80000000 eb936702 c19d148f


The proper hit seems to be

HIT!!!!
1318086158704845 00000000 f3a8d619 1b8eb870 B[ 75]
1793950977996770 00000000 f3a8d619 1b8eb870 B[125]
Fast forward first by 1288564513

Window size is 2949434999
Hit 1317259748800005[75]B 1793375838171965[125]B, delta 2556517787
6a55ac0c b4ee0085 ac44eb3c -> 10180266 62f2c61a 1e3fd422
7cd9d49d 30f2f9c9 79260962 -> 10180266 62f2c61a 1e3fd422


H 1318086158704845 1 1793950977996770 1 2556517787 6a55ac0c b4ee0085 ac44eb3c 10180266 62f2c61a 1e3fd422 7cd9d49d 30f2f9c9 79260962 10180266 62f2c61a 1e3fd422


*/

int main()
{
    assert (LOWA + GAPA * 195 == HIGHA);
    assert (LOWB + GAPB * 195 == HIGHB);

    uint32_t dataA[3] = { 0, 0xaec6f9d1, 0x24eef6cc };

    uint64_t i;
    for (i = 0; i != GAPA; ++i) {
        if ((dataA[0] & TRIGGER_MASK) == 0)
            printf ("At %lu\n" "R %lu %x %x %x\n",
                    i, LOWA + 195 * i, dataA[0], dataA[1], dataA[2]);
        transform (dataA, dataA);
    }
    printf ("At end:\n" "R %lu %x %x %x\n",
            LOWA + 195 * i, dataA[0], dataA[1], dataA[2]);

    return 0;
}
