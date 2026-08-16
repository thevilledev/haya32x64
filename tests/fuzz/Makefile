CC       ?= clang
CFLAGS   ?= -O1 -g -std=c11 -Wall -Wextra -Werror \
	-fsanitize=fuzzer,address,undefined \
	-fno-omit-frame-pointer \
	-fno-sanitize-recover=all
FUZZ_SECONDS ?= 60

.PHONY: all fuzz-run clean

all: one_shot_stream

one_shot_stream: one_shot_stream.c ../../haya32x64.h
	$(CC) $(CFLAGS) -o $@ one_shot_stream.c

fuzz-run: one_shot_stream
	mkdir -p artifacts
	./one_shot_stream -max_total_time=$(FUZZ_SECONDS) \
		-artifact_prefix=artifacts/

clean:
	rm -f one_shot_stream
	rm -rf artifacts
