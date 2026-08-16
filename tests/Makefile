CC       ?= cc
CFLAGS   ?= -O2 -std=c11 -Wall -Wextra -Werror
CPPFLAGS ?=

.PHONY: all test check-kat regen-kat generate clean

all: kat verify quality streaming

kat: kat.c ../haya32x64.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ kat.c

verify: verify.c ../haya32x64.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ verify.c

quality: quality.c ../haya32x64.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ quality.c -lm

streaming: streaming.c ../haya32x64.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ streaming.c

check-kat: kat kat.txt
	./kat > kat.actual.txt
	diff -u kat.txt kat.actual.txt
	rm -f kat.actual.txt

regen-kat: kat
	./kat > kat.txt

generate: differential/generate

differential/generate: differential/generate.c ../haya32x64.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ differential/generate.c

test: check-kat verify quality streaming
	./verify
	./quality
	./streaming

clean:
	rm -f kat verify quality streaming kat.actual.txt differential/generate
	rm -rf kat.dSYM verify.dSYM quality.dSYM streaming.dSYM
