#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../haya32x64.h"

enum { MAX_LENGTH = 65536 };

static uint64_t random_state = UINT64_C(0xD1B54A32D192ED03);

static uint64_t random64(void)
{
	uint64_t x = (random_state += UINT64_C(0x9E3779B97F4A7C15));
	x ^= x >> 30;
	x *= UINT64_C(0xBF58476D1CE4E5B9);
	x ^= x >> 27;
	x *= UINT64_C(0x94D049BB133111EB);
	return x ^ (x >> 31);
}

static int equal(haya32x64_words a, haya32x64_words b)
{
	return a.lo == b.lo && a.hi == b.hi;
}

static void fail(size_t length, unsigned int pattern,
		 haya32x64_words expected, haya32x64_words actual)
{
	fprintf(stderr,
		"stream mismatch: len=%zu pattern=%u "
		"expected=%08" PRIx32 "%08" PRIx32 " "
		"actual=%08" PRIx32 "%08" PRIx32 "\n",
		length, pattern, expected.hi, expected.lo, actual.hi, actual.lo);
	exit(1);
}

static void check_chunks(const uint8_t *key, size_t length,
			 uint32_t seed_lo, uint32_t seed_hi,
			 const size_t *chunks, size_t chunk_count,
			 unsigned int pattern)
{
	haya32x64_words expected = haya32x64_hash(
		key, length, seed_lo, seed_hi);
	haya32x64_state state;
	haya32x64_init(&state, seed_lo, seed_hi);
	haya32x64_update(&state, NULL, 0);
	size_t offset = 0;
	size_t index = 0;
	while (offset < length) {
		size_t chunk = chunks[index++ % chunk_count];
		if (chunk > length - offset)
			chunk = length - offset;
		haya32x64_update(&state, key + offset, chunk);
		offset += chunk;
	}
	haya32x64_words actual = haya32x64_digest(&state);
	if (!equal(expected, actual))
		fail(length, pattern, expected, actual);
	if (!equal(actual, haya32x64_digest(&state)))
		fail(length, pattern, actual, haya32x64_digest(&state));
}

static void exhaustive(const uint8_t *key)
{
	static const size_t one[] = {1};
	static const size_t three[] = {3};
	static const size_t stripe[] = {4};
	static const size_t block[] = {32};
	static const size_t awkward[] = {1, 31, 2, 30, 3, 29, 5, 17, 64, 7};
	for (size_t length = 0; length <= 2048; length++) {
		uint32_t seed_lo = (uint32_t)random64();
		uint32_t seed_hi = (uint32_t)random64();
		check_chunks(key, length, seed_lo, seed_hi, one, 1, 1);
		check_chunks(key, length, seed_lo, seed_hi, three, 1, 2);
		check_chunks(key, length, seed_lo, seed_hi, stripe, 1, 3);
		check_chunks(key, length, seed_lo, seed_hi, block, 1, 4);
		check_chunks(key, length, seed_lo, seed_hi,
			     awkward, sizeof(awkward) / sizeof(awkward[0]), 5);
		for (size_t split = 0; split <= length; split++) {
			haya32x64_state state;
			haya32x64_init(&state, seed_lo, seed_hi);
			haya32x64_update(&state, key, split);
			haya32x64_words prefix = haya32x64_hash(
				key, split, seed_lo, seed_hi);
			if (!equal(prefix, haya32x64_digest(&state)))
				fail(split, 6, prefix, haya32x64_digest(&state));
			haya32x64_update(&state, key + split, length - split);
			haya32x64_words expected = haya32x64_hash(
				key, length, seed_lo, seed_hi);
			if (!equal(expected, haya32x64_digest(&state)))
				fail(length, 6, expected, haya32x64_digest(&state));
		}
	}
}

static void randomized(const uint8_t *key)
{
	for (unsigned int trial = 0; trial < 10000; trial++) {
		size_t length = (size_t)(random64() % (MAX_LENGTH + 1));
		uint32_t seed_lo = (uint32_t)random64();
		uint32_t seed_hi = (uint32_t)random64();
		haya32x64_words expected = haya32x64_hash(
			key, length, seed_lo, seed_hi);
		haya32x64_state state;
		haya32x64_init(&state, seed_lo, seed_hi);
		size_t offset = 0;
		while (offset < length) {
			size_t chunk = 1 + (size_t)(random64() % 1024);
			if (chunk > length - offset)
				chunk = length - offset;
			haya32x64_update(&state, key + offset, chunk);
			offset += chunk;
			if ((random64() & 15) == 0) {
				haya32x64_words prefix = haya32x64_hash(
					key, offset, seed_lo, seed_hi);
				if (!equal(prefix, haya32x64_digest(&state)))
					fail(offset, 7, prefix, haya32x64_digest(&state));
			}
		}
		if (!equal(expected, haya32x64_digest(&state)))
			fail(length, 7, expected, haya32x64_digest(&state));
	}
}

int main(void)
{
	uint8_t *key = (uint8_t *)malloc(MAX_LENGTH);
	if (key == NULL)
		return 2;
	for (size_t i = 0; i < MAX_LENGTH; i++)
		key[i] = (uint8_t)random64();
	exhaustive(key);
	randomized(key);
	haya32x64_state state;
	uint64_t seed = UINT64_C(0xDEADBEEFCAFEBABE);
	haya32x64_init64(&state, seed);
	haya32x64_update(&state, key, 4096);
	if (haya32x64_digest64(&state) != haya32x64(key, 4096, seed)) {
		fputs("64-bit convenience API mismatch\n", stderr);
		free(key);
		return 1;
	}
	free(key);
	puts("haya32x64 streaming: PASSED");
	return 0;
}
