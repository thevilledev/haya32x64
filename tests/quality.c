// Fast local quality ladder.  This is deliberately smaller than SMHasher3,
// but it covers the structural key classes that shaped haya32x64, including
// the sparse-seed birthday battery that caught the prototype seed premix.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../haya32x64.h"

static uint64_t random_state = UINT64_C(0x853C49E6748FEA9B);

static uint64_t random64(void)
{
	uint64_t x = (random_state += UINT64_C(0x9E3779B97F4A7C15));
	x ^= x >> 30;
	x *= UINT64_C(0xBF58476D1CE4E5B9);
	x ^= x >> 27;
	x *= UINT64_C(0x94D049BB133111EB);
	return x ^ (x >> 31);
}

static void fill_random(uint8_t *bytes, size_t length)
{
	for (size_t i = 0; i < length; i++)
		bytes[i] = (uint8_t)random64();
}

static uint64_t *digests;
static size_t digest_count;
static size_t digest_capacity;

static int compare_u64(const void *left, const void *right)
{
	uint64_t a = *(const uint64_t *)left;
	uint64_t b = *(const uint64_t *)right;
	return (a > b) - (a < b);
}

static void collision_reset(size_t capacity)
{
	if (capacity > digest_capacity) {
		uint64_t *grown = realloc(digests, capacity * sizeof(*digests));
		if (grown == NULL) {
			fprintf(stderr, "cannot allocate %zu collision slots\n", capacity);
			exit(2);
		}
		digests = grown;
		digest_capacity = capacity;
	}
	digest_count = 0;
}

static void collision_add(uint64_t digest)
{
	if (digest_count >= digest_capacity) {
		fprintf(stderr, "collision buffer overflow\n");
		exit(2);
	}
	digests[digest_count++] = digest;
}

static size_t collision_count(void)
{
	qsort(digests, digest_count, sizeof(*digests), compare_u64);
	size_t collisions = 0;
	for (size_t i = 1; i < digest_count; i++)
		collisions += digests[i] == digests[i - 1];
	return collisions;
}

static double sac_input(size_t length, unsigned int trials)
{
	enum { POSITIONS = 64 };
	uint32_t counts[POSITIONS][64] = {{0}};
	uint8_t *key = malloc(length ? length : 1);
	if (key == NULL)
		exit(2);
	size_t bits = length * 8;
	size_t positions = bits < POSITIONS ? bits : POSITIONS;
	for (unsigned int trial = 0; trial < trials; trial++) {
		fill_random(key, length);
		uint64_t seed = random64();
		uint64_t base = haya32x64(key, length, seed);
		for (size_t p = 0; p < positions; p++) {
			size_t bit = bits <= POSITIONS ? p :
				(p * (bits - 1)) / (POSITIONS - 1);
			key[bit >> 3] ^= (uint8_t)(1u << (bit & 7));
			uint64_t delta = base ^ haya32x64(key, length, seed);
			key[bit >> 3] ^= (uint8_t)(1u << (bit & 7));
			for (unsigned int out = 0; out < 64; out++)
				counts[p][out] += (uint32_t)((delta >> out) & 1);
		}
	}
	free(key);
	double worst = 0.0;
	for (size_t p = 0; p < positions; p++) {
		for (unsigned int out = 0; out < 64; out++) {
			double bias = fabs((double)counts[p][out] / trials - 0.5);
			if (bias > worst)
				worst = bias;
		}
	}
	return worst;
}

static double sac_seed(size_t length, unsigned int trials)
{
	uint32_t counts[64][64] = {{0}};
	uint8_t *key = malloc(length ? length : 1);
	if (key == NULL)
		exit(2);
	for (unsigned int trial = 0; trial < trials; trial++) {
		fill_random(key, length);
		uint64_t seed = random64();
		uint64_t base = haya32x64(key, length, seed);
		for (unsigned int bit = 0; bit < 64; bit++) {
			uint64_t delta = base ^ haya32x64(
				key, length, seed ^ (UINT64_C(1) << bit));
			for (unsigned int out = 0; out < 64; out++)
				counts[bit][out] += (uint32_t)((delta >> out) & 1);
		}
	}
	free(key);
	double worst = 0.0;
	for (unsigned int bit = 0; bit < 64; bit++) {
		for (unsigned int out = 0; out < 64; out++) {
			double bias = fabs((double)counts[bit][out] / trials - 0.5);
			if (bias > worst)
				worst = bias;
		}
	}
	return worst;
}

// All seeds with Hamming weight <= 4: 679,121 distinct 64-bit seeds.  A lossy
// 32-bit premix produces birthday collisions here; the two-round Feistel
// premix must keep the set collision-free at every fixed (key, length).
static size_t test_seed_sparse(void)
{
	enum { COUNT = 1 + 64 + 2016 + 41664 + 635376 };
	static const uint8_t key[] = "seed-sparse-premix-regression";
	collision_reset(COUNT);
	collision_add(haya32x64(key, sizeof(key) - 1, 0));
	for (unsigned int a = 0; a < 64; a++) {
		uint64_t sa = UINT64_C(1) << a;
		collision_add(haya32x64(key, sizeof(key) - 1, sa));
		for (unsigned int b = a + 1; b < 64; b++) {
			uint64_t sb = sa | (UINT64_C(1) << b);
			collision_add(haya32x64(key, sizeof(key) - 1, sb));
			for (unsigned int c = b + 1; c < 64; c++) {
				uint64_t sc = sb | (UINT64_C(1) << c);
				collision_add(haya32x64(key, sizeof(key) - 1, sc));
				for (unsigned int d = c + 1; d < 64; d++) {
					uint64_t sd = sc | (UINT64_C(1) << d);
					collision_add(haya32x64(
						key, sizeof(key) - 1, sd));
				}
			}
		}
	}
	if (digest_count != COUNT) {
		fprintf(stderr, "seed-sparse count error: %zu != %d\n",
		        digest_count, COUNT);
		exit(2);
	}
	return collision_count();
}

static size_t test_sequential(size_t length, size_t count)
{
	uint8_t key[64] = {0};
	collision_reset(count);
	for (size_t i = 0; i < count; i++) {
		uint64_t value = i;
		memcpy(key, &value, sizeof(value));
		collision_add(haya32x64(key, length, 0));
	}
	return collision_count();
}

// Only the high byte of each 32-bit stripe varies.  This stresses the exact
// flaw class addressed by harvesting the high product word.
static size_t test_high_bytes(size_t count)
{
	uint8_t key[64] = {0};
	collision_reset(count);
	for (size_t i = 0; i < count; i++) {
		uint64_t value = i;
		for (size_t stripe = 0; stripe < 8; stripe++)
			key[stripe * 4 + 3] = (uint8_t)(value >> (stripe * 8));
		collision_add(haya32x64(key, sizeof(key), 0));
	}
	return collision_count();
}

static size_t test_zero_extension(size_t maximum)
{
	uint8_t *key = calloc(maximum, 1);
	if (key == NULL)
		exit(2);
	collision_reset(maximum + 1);
	for (size_t length = 0; length <= maximum; length++)
		collision_add(haya32x64(key, length, 0));
	free(key);
	return collision_count();
}

// Concatenated 32-bit blocks chosen from 0 and 0x80000000.  Differences live
// only at the top of each word and expose linear or lossy absorb structures.
static size_t test_combinations(void)
{
	enum { BLOCKS = 18, COUNT = (1 << (BLOCKS + 1)) - 2 };
	uint8_t key[BLOCKS * 4];
	collision_reset(COUNT);
	for (unsigned int blocks = 1; blocks <= BLOCKS; blocks++) {
		for (uint32_t pattern = 0; pattern < (UINT32_C(1) << blocks); pattern++) {
			memset(key, 0, sizeof(key));
			for (unsigned int block = 0; block < blocks; block++) {
				if (pattern & (UINT32_C(1) << block))
					key[block * 4 + 3] = 0x80;
			}
			collision_add(haya32x64(key, blocks * 4, UINT64_C(0x1234)));
		}
	}
	return collision_count();
}

static int report_collisions(const char *name, size_t collisions)
{
	printf("  %-24s %s", name, collisions == 0 ? "clean" : "FAILED");
	if (collisions != 0)
		printf(" (%zu collisions)", collisions);
	putchar('\n');
	return collisions != 0;
}

int main(void)
{
	int failures = 0;
	static const size_t input_lengths[] = {
		1, 3, 4, 8, 9, 16, 17, 31, 32, 64, 127, 128, 129, 256, 1024,
	};
	static const size_t seed_lengths[] = {0, 1, 8, 9, 16, 32, 128, 1024};
	double worst_input = 0.0;
	double worst_seed = 0.0;

	puts("== avalanche ==");
	for (size_t i = 0; i < sizeof(input_lengths) / sizeof(input_lengths[0]); i++) {
		double bias = sac_input(input_lengths[i], 1500);
		if (bias > worst_input)
			worst_input = bias;
	}
	for (size_t i = 0; i < sizeof(seed_lengths) / sizeof(seed_lengths[0]); i++) {
		double bias = sac_seed(seed_lengths[i], 1500);
		if (bias > worst_seed)
			worst_seed = bias;
	}
	printf("  worst input-bit bias: %.4f\n", worst_input);
	printf("  worst seed-bit bias:  %.4f\n", worst_seed);
	if (worst_input > 0.075 || worst_seed > 0.075) {
		puts("  avalanche threshold FAILED");
		failures++;
	}

	puts("== exact collisions ==");
	failures += report_collisions("seed-sparse weight <= 4", test_seed_sparse());
	failures += report_collisions("sequential len 4", test_sequential(4, 300000));
	failures += report_collisions("sequential len 16", test_sequential(16, 300000));
	failures += report_collisions("sequential len 64", test_sequential(64, 300000));
	failures += report_collisions("high-byte stripes", test_high_bytes(300000));
	failures += report_collisions("zero extension", test_zero_extension(100000));
	failures += report_collisions("top-bit combinations", test_combinations());

	free(digests);
	printf("== haya32x64: %s ==\n", failures == 0 ? "PASSED" : "FAILED");
	return failures == 0 ? 0 : 1;
}
