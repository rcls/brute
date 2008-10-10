
include ../Rules.mk

CFLAGS=-O2 -flax-vector-conversions -msse -msse2 -march=athlon64 -mtune=athlon64 -Wall -Winline -Werror -std=gnu99 -fomit-frame-pointer -fsched-stalled-insns=100000 -fsched-stalled-insns-dep=100000 -fpredictive-commoning --param salias-max-array-elements=10 --param max-pending-list-length=100000



all: brute

brute: brute.o -lpthread
#brute LIBS = -lcrypto

%.s: %.c
	$(COMPILE) -S -o $@ $<

brute3.c: brute.c
	cp brute.c brute3.c

brute3: brute3.o
brute3: GCC=/home/ralph/dev/gcc-3.4.3/bin/gcc

brute-trunk.c: brute.c
	cp brute.c brute-trunk.c
brute-trunk: GCC=/home/ralph/dev/gcc/bin/gcc

.PHONY: clean all
clean:
	rm -f *.o */.deps/*.d *.memlog *.i *.s
	rm -f brute
	rm -f *.a *.so *.so.*

-include .deps/*.d
