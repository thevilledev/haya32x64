#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include "../haya32x64.h"

// Reproduces SMHasher3's verification-code construction for a 64-bit hash.
int main(void)
{
	uint8_t key[256] = {0};
	uint8_t hashes[256 * 8];
	for (uint32_t i = 0; i < 256; i++) {
		haya32x64_words digest = haya32x64_hash(key, i, 256 - i, 0);
		for (unsigned int b = 0; b < 4; b++) {
			hashes[i * 8 + b] = (uint8_t)(digest.lo >> (b * 8));
			hashes[i * 8 + 4 + b] = (uint8_t)(digest.hi >> (b * 8));
		}
		key[i] = (uint8_t)i;
	}
	haya32x64_words final = haya32x64_hash(hashes, sizeof(hashes), 0, 0);
	printf("%08" PRIx32 "\n", final.lo);
	if (final.lo != UINT32_C(0xEAA8E435)) {
		fprintf(stderr, "verification mismatch: expected eaa8e435\n");
		return 1;
	}
	return 0;
}
