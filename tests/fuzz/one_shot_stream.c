// libFuzzer target: one-shot digest must match streaming splits.
// A mismatch aborts so the sanitizer records a crashing input.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "../../haya32x64.h"

static int equal(haya32x64_words a, haya32x64_words b)
{
	return a.lo == b.lo && a.hi == b.hi;
}

static haya32x64_words stream_chunks(
	const uint8_t *bytes, size_t length,
	uint32_t seed_lo, uint32_t seed_hi,
	const size_t *chunks, size_t chunk_count)
{
	haya32x64_state state;
	haya32x64_init(&state, seed_lo, seed_hi);
	size_t offset = 0;
	size_t index = 0;
	while (offset < length) {
		size_t chunk = chunks[index++ % chunk_count];
		if (chunk > length - offset)
			chunk = length - offset;
		if (chunk == 0)
			chunk = 1;
		haya32x64_update(&state, bytes + offset, chunk);
		offset += chunk;
	}
	haya32x64_words first = haya32x64_digest(&state);
	haya32x64_words second = haya32x64_digest(&state);
	if (!equal(first, second))
		__builtin_trap();
	return first;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 8)
		return 0;

	uint32_t seed_lo;
	uint32_t seed_hi;
	memcpy(&seed_lo, data, 4);
	memcpy(&seed_hi, data + 4, 4);
	const uint8_t *bytes = data + 8;
	size_t length = size - 8;

	haya32x64_words one_shot = haya32x64_hash(
		bytes, length, seed_lo, seed_hi);

	size_t whole[] = {length ? length : 1};
	size_t mid[] = {length / 2, length - length / 2};
	size_t thirds[] = {
		length / 3,
		length / 3,
		length - 2 * (length / 3),
	};
	size_t awkward[] = {1, 3, 4, 7, 16, 31, 32, 33, 63, 64, 127, 128};

	if (!equal(one_shot, stream_chunks(
		    bytes, length, seed_lo, seed_hi, whole, 1)))
		__builtin_trap();
	if (length > 0 && !equal(one_shot, stream_chunks(
		    bytes, length, seed_lo, seed_hi, mid, 2)))
		__builtin_trap();
	if (length > 2 && !equal(one_shot, stream_chunks(
		    bytes, length, seed_lo, seed_hi, thirds, 3)))
		__builtin_trap();
	if (!equal(one_shot, stream_chunks(
		    bytes, length, seed_lo, seed_hi,
		    awkward, sizeof(awkward) / sizeof(awkward[0]))))
		__builtin_trap();
	return 0;
}
