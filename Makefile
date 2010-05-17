
CFLAGS=-O3 -g -ffast-math -flax-vector-conversions -msse -msse2 -march=native -mtune=native -Wall -Wshadow -Winline -Werror -std=gnu99 -MMD -MP -MF.deps/$(subst /,:,$@).d

vpath %.so /usr/lib64

all: md5log check collate collate-check

md5log: -lm

check: jtag-io.o -lcrypto

collate: jtag-io.o -lcrypto -lpthread

bizarre: jtag-io.o -lcrypto

#all: brute

brute: -lpthread
#brute LIBS = -lcrypto

collate-check: jtag-io.o -lcrypto -lpthread -lm

%.s: %.c
	$(COMPILE.c) -S -o $@ $<

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
	rm -f brute collate check
	rm -f *.a *.so *.so.*

-include .deps/*.d

%: %.c