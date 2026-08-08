CC       ?= cc
CFLAGS   ?= -O2 -std=c11 -Wall -Wextra -Werror
CPPFLAGS ?=

.PHONY: all test check-kat clean

all: kat verify quality

kat: kat.c ../haya32x64.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ kat.c

verify: verify.c ../haya32x64.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ verify.c

quality: quality.c ../haya32x64.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ quality.c -lm

check-kat: kat kat.txt
	./kat > kat.actual.txt
	diff -u kat.txt kat.actual.txt
	rm -f kat.actual.txt

test: check-kat verify quality
	./verify
	./quality

clean:
	rm -f kat verify quality kat.actual.txt differential/generate
