#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../haya32x64.h"

enum {
	DEFAULT_CASES = 4096,
	EXHAUSTIVE_LENGTH = 384,
	MAX_LENGTH = 128 * 1024,
};

struct prng {
	uint64_t state;
};

static uint64_t next64(struct prng *random)
{
	uint64_t x = (random->state += UINT64_C(0x9E3779B97F4A7C15));
	x ^= x >> 30;
	x *= UINT64_C(0xBF58476D1CE4E5B9);
	x ^= x >> 27;
	x *= UINT64_C(0x94D049BB133111EB);
	return x ^ (x >> 31);
}

static uint64_t parse64(const char *text, const char *name)
{
	char *end;
	errno = 0;
	unsigned long long value = strtoull(text, &end, 0);
	if (errno != 0 || *text == '\0' || *end != '\0') {
		fprintf(stderr, "invalid %s: %s\n", name, text);
		exit(2);
	}
	return (uint64_t)value;
}

static uint32_t parse_count(const char *text)
{
	uint64_t count = parse64(text, "case count");
	if (count < EXHAUSTIVE_LENGTH + 1 || count > UINT32_MAX) {
		fprintf(stderr, "case count must be in [%d, %" PRIu32 "]\n",
		        EXHAUSTIVE_LENGTH + 1, UINT32_MAX);
		exit(2);
	}
	return (uint32_t)count;
}

static int write_bytes(FILE *file, const void *data, size_t length)
{
	if (length == 0)
		return 1;
	return fwrite(data, 1, length, file) == length;
}

static int write32(FILE *file, uint32_t value)
{
	uint8_t bytes[4];
	for (unsigned int i = 0; i < 4; i++)
		bytes[i] = (uint8_t)(value >> (i * 8));
	return write_bytes(file, bytes, sizeof(bytes));
}

static uint32_t case_length(uint32_t index, struct prng *random)
{
	static const uint32_t edges[] = {
		127, 128, 129, 255, 256, 257, 1023, 1024, 4095, 4096,
		16383, 16384, 65535, 65536, 131071, 131072,
	};
	if (index <= EXHAUSTIVE_LENGTH)
		return index;
	index -= EXHAUSTIVE_LENGTH + 1;
	if (index < sizeof(edges) / sizeof(edges[0]))
		return edges[index];
	uint64_t choice = next64(random) & 15;
	if (choice < 7)
		return (uint32_t)(next64(random) % 385);
	if (choice < 12)
		return (uint32_t)(next64(random) % 4097);
	if (choice < 15)
		return (uint32_t)(next64(random) % 16385);
	return (uint32_t)(next64(random) % (MAX_LENGTH + 1));
}

int main(int argc, char **argv)
{
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "usage: %s OUTPUT PRNG_SEED [CASE_COUNT]\n", argv[0]);
		return 2;
	}
	uint64_t master_seed = parse64(argv[2], "PRNG seed");
	uint32_t cases = argc == 4 ? parse_count(argv[3]) : DEFAULT_CASES;
	struct prng random = {master_seed};
	FILE *output = fopen(argv[1], "wb");
	if (output == NULL) {
		fprintf(stderr, "cannot open %s: %s\n", argv[1], strerror(errno));
		return 1;
	}
	uint8_t *input = NULL;
	size_t capacity = 0;
	int ok = write_bytes(output, "H32XFZ01", 8) &&
		write32(output, cases) &&
		write32(output, (uint32_t)master_seed) &&
		write32(output, (uint32_t)(master_seed >> 32));
	for (uint32_t i = 0; ok && i < cases; i++) {
		uint32_t length = case_length(i, &random);
		uint64_t seed = next64(&random);
		if (length > capacity) {
			uint8_t *grown = realloc(input, length);
			if (grown == NULL) {
				ok = 0;
				break;
			}
			input = grown;
			capacity = length;
		}
		for (uint32_t j = 0; j < length; j++)
			input[j] = (uint8_t)next64(&random);
		haya32x64_words digest = haya32x64_hash(
			input, length, (uint32_t)seed, (uint32_t)(seed >> 32));
		ok = write32(output, length) &&
			write32(output, (uint32_t)seed) &&
			write32(output, (uint32_t)(seed >> 32)) &&
			write32(output, digest.lo) &&
			write32(output, digest.hi) &&
			write_bytes(output, input, length);
	}
	free(input);
	if (fclose(output) != 0)
		ok = 0;
	if (!ok) {
		fprintf(stderr, "failed to write corpus %s\n", argv[1]);
		return 1;
	}
	fprintf(stderr, "generated %" PRIu32 " cases from seed 0x%016" PRIx64 "\n",
	        cases, master_seed);
	return 0;
}
