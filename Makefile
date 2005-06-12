
include ../Rules.mk

CFLAGS += -g3 -Winline -fomit-frame-pointer -mtune=i686 -march=i686 -mmmx

all: brute

brute: brute.o
#brute LIBS = -lcrypto

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
