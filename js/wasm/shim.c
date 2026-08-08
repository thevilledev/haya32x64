#include <stddef.h>
#include <stdint.h>

#include "../../haya32x64.h"

// Freestanding replacement for the header's fixed-size alignment-safe loads.
__attribute__((no_builtin("memcpy")))
void *memcpy(void *destination, const void *source, size_t length)
{
	unsigned char *out = destination;
	const unsigned char *in = source;
	for (size_t i = 0; i < length; i++)
		out[i] = in[i];
	return destination;
}

// Referenced by the header's streaming buffer management.  The wasm wrapper
// currently exports only one-shot hashing, so the linker drops this until a
// streaming wasm engine is added.
__attribute__((no_builtin("memmove")))
void *memmove(void *destination, const void *source, size_t length)
{
	unsigned char *out = destination;
	const unsigned char *in = source;
	if (out < in) {
		for (size_t i = 0; i < length; i++)
			out[i] = in[i];
	} else {
		for (size_t i = length; i > 0; i--)
			out[i - 1] = in[i - 1];
	}
	return destination;
}

// All arguments and the result pointer are i32 at the wasm boundary.  This
// keeps the JavaScript wrapper free of BigInt even when it uses wasm.
__attribute__((export_name("haya32x64")))
void wasm_haya32x64(haya32x64_words *output, const uint8_t *input,
		    uint32_t length, uint32_t seed_lo, uint32_t seed_hi)
{
	*output = haya32x64_hash(input, length, seed_lo, seed_hi);
}
