
include ../Rules.mk

all: brute

brute: brute.o
brute LIBS = -lcrypto

.PHONY: clean all
clean:
	rm -f *.o */.deps/*.d *.memlog *.i *.s
	rm -f brute
	rm -f *.a *.so *.so.*

-include .deps/*.d
