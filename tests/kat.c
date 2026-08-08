#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "../haya32x64.h"

static const uint32_t lengths[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 20, 23, 24, 25, 31, 32, 33, 47, 48, 49, 63, 64,
	65, 79, 80, 95, 96, 111, 112, 127, 128, 129, 143, 144,
	159, 160, 191, 192, 223, 224, 255, 256, 257, 383, 384, 511,
	512, 1023, 1024, 2047, 2048, 4096,
};

static const haya32x64_words seeds[] = {
	{UINT32_C(0x00000000), UINT32_C(0x00000000)},
	{UINT32_C(0x7F4A7C15), UINT32_C(0x9E3779B9)},
	{UINT32_C(0xCAFEBABE), UINT32_C(0xDEADBEEF)},
};

static uint8_t byte_at(uint32_t index)
{
	return (uint8_t)(
		(index * UINT32_C(0x9E3779B1) + UINT32_C(0x7F4A7C15)) >> 24);
}

int main(void)
{
	uint8_t key[4096];
	for (uint32_t i = 0; i < sizeof(key); i++)
		key[i] = byte_at(i);

	puts("# len seed_hi seed_lo digest_hi digest_lo");
	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
		for (size_t j = 0; j < sizeof(seeds) / sizeof(seeds[0]); j++) {
			haya32x64_words digest = haya32x64_hash(
				key, lengths[i], seeds[j].lo, seeds[j].hi);
			printf("%" PRIu32 " %08" PRIx32 " %08" PRIx32
			       " %08" PRIx32 " %08" PRIx32 "\n",
			       lengths[i], seeds[j].hi, seeds[j].lo,
			       digest.hi, digest.lo);
		}
	}
	return 0;
}
