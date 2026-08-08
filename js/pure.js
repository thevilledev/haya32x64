// Bit-exact pure-JavaScript port of haya32x64.h.  The hot path uses only
// int32 bitwise operations, Math.imul, and exact 16-bit-limb arithmetic for
// the high half of a 32x32 product.  There is no BigInt or WebAssembly here.

const KA = 0x9e3779b1 | 0;
const KB = 0x85ebca77 | 0;
const KC = 0xc2b2ae3d | 0;
const KD = 0x27d4eb2f | 0;
const KE = 0x165667b1 | 0;
const F1 = 0x85ebca6b | 0;
const F2 = 0xc2b2ae35 | 0;
const KA_LO = 0x79b1;
const KA_HI = 0x9e37;

// Exact high word of u * KA.  Every intermediate remains an exact integer
// within JavaScript Number's 53-bit range; unsigned shifts split limbs.
function mulhiKA(u) {
	const lo = u & 0xffff;
	const hi = u >>> 16;
	const cross0 = lo * KA_HI + ((lo * KA_LO) >>> 16);
	const cross1 = hi * KA_LO;
	return (hi * KA_HI + (cross0 >>> 16) + (cross1 >>> 16) +
		((((cross0 & 0xffff) + (cross1 & 0xffff)) >>> 16))) | 0;
}

// Exact high word of a general unsigned 32x32 product.
function mulhi(a, b) {
	const a0 = a & 0xffff;
	const a1 = a >>> 16;
	const b0 = b & 0xffff;
	const b1 = b >>> 16;
	const x = a1 * b0 + ((a0 * b0) >>> 16);
	const y = a0 * b1 + (x & 0xffff);
	return (a1 * b1 + (x >>> 16) + (y >>> 16)) | 0;
}

function rotl(x, n) {
	return (x << n) | (x >>> (32 - n));
}

function inja(w) {
	return w ^ rotl(w, 10) ^ rotl(w, 21);
}

function injb(w) {
	return w ^ rotl(w, 6) ^ rotl(w, 25);
}

function final4(h0, h1, h2, h3, s0, s1, carry) {
	h1 ^= carry;
	h3 ^= rotl(carry, 16);
	const a0 = h0 ^ s0;
	const a1 = h2 ^ KC;
	const b0 = h1 ^ KD;
	const b1 = h3 ^ rotl(s1, 11);
	let x = Math.imul(a0, a1) ^ mulhi(b0, b1) ^ rotl(h1, 7);
	let y = mulhi(a0, a1) ^ Math.imul(b0, b1) ^ rotl(h0, 19);
	x ^= x >>> 16;
	x = Math.imul(x, F1);
	y ^= y >>> 15;
	y = Math.imul(y, F2);
	x ^= rotl(y, 13);
	y ^= x >>> 16;
	x = Math.imul(x, KB);
	x ^= x >>> 15;
	y = Math.imul(y, KE);
	y ^= y >>> 14;
	return [x >>> 0, y >>> 0];
}

/**
 * Hashes bytes using the pure-JavaScript engine.
 * @param {Uint8Array} bytes
 * @param {number} seedLo
 * @param {number} seedHi
 * @returns {[number, number]} digest words in [low, high] order
 */
export function hashPure(bytes, seedLo = 0, seedHi = 0) {
	let remaining = bytes.length;
	let offset = 0;

	// Two-round Feistel seed premix; bijective in (seedLo, seedHi).
	const q1a = seedHi ^ KC;
	const u0 = seedLo ^ Math.imul(q1a, KD) ^ mulhi(q1a, KD);
	const q0a = u0 ^ KA;
	const v0 = seedHi ^ Math.imul(q0a, KB) ^ mulhi(q0a, KB);
	const q2a = (remaining + KE) | 0;
	const s0 = u0 ^ Math.imul(q2a, KA);
	const s1 = v0 ^ mulhiKA(q2a);

	if (remaining <= 8) {
		let a;
		let b;
		if (remaining >= 4) {
			a = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) |
				(bytes[3] << 24);
			b = bytes[remaining - 4] | (bytes[remaining - 3] << 8) |
				(bytes[remaining - 2] << 16) |
				(bytes[remaining - 1] << 24);
		} else if (remaining > 0) {
			a = bytes[0];
			b = (bytes[remaining >> 1] << 8) |
				(bytes[remaining - 1] << 16);
		} else {
			a = 0;
			b = 0;
		}
		const xa = inja(a) ^ s0 ^ KB;
		const yb = injb(b) ^ rotl(s1, 13) ^ KE;
		return final4(
			Math.imul(xa, KA), mulhiKA(xa),
			Math.imul(yb, KC), mulhi(yb, KC),
			s0, s1, 0,
		);
	}

	const view = new DataView(
		bytes.buffer,
		bytes.byteOffset,
		bytes.byteLength,
	);
	let h0 = s0 ^ KA;
	let h1 = (rotl(s1, 7) + KB) | 0;
	let h2 = rotl(s0, 14) ^ KC;
	let h3 = (rotl(s1, 21) + KD) | 0;
	let word = 0;
	let previous = 0;
	let carry = 0;
	let laneInput = 0;

	if (remaining >= 128) {
		let h4 = (s1 + KE) | 0;
		let h5 = rotl(s0, 9) ^ KD;
		let h6 = (rotl(s1, 18) + KA) | 0;
		let h7 = rotl(s0, 27) ^ KB;
		do {
			word = view.getUint32(offset, true) | 0;
			laneInput = h0 ^ ((word + rotl(previous, 11)) | 0);
			h0 = Math.imul(laneInput, KA);
			carry = rotl(carry, 5) ^ mulhiKA(laneInput);
			previous = word;
			word = view.getUint32(offset + 4, true) | 0;
			laneInput = h1 ^ ((word + rotl(previous, 11)) | 0);
			h1 = Math.imul(laneInput, KA);
			carry = rotl(carry, 5) ^ mulhiKA(laneInput);
			previous = word;
			word = view.getUint32(offset + 8, true) | 0;
			laneInput = h2 ^ ((word + rotl(previous, 11)) | 0);
			h2 = Math.imul(laneInput, KA);
			carry = rotl(carry, 5) ^ mulhiKA(laneInput);
			previous = word;
			word = view.getUint32(offset + 12, true) | 0;
			laneInput = h3 ^ ((word + rotl(previous, 11)) | 0);
			h3 = Math.imul(laneInput, KA);
			carry = rotl(carry, 5) ^ mulhiKA(laneInput);
			previous = word;
			word = view.getUint32(offset + 16, true) | 0;
			laneInput = h4 ^ ((word + rotl(previous, 11)) | 0);
			h4 = Math.imul(laneInput, KA);
			carry = rotl(carry, 5) ^ mulhiKA(laneInput);
			previous = word;
			word = view.getUint32(offset + 20, true) | 0;
			laneInput = h5 ^ ((word + rotl(previous, 11)) | 0);
			h5 = Math.imul(laneInput, KA);
			carry = rotl(carry, 5) ^ mulhiKA(laneInput);
			previous = word;
			word = view.getUint32(offset + 24, true) | 0;
			laneInput = h6 ^ ((word + rotl(previous, 11)) | 0);
			h6 = Math.imul(laneInput, KA);
			carry = rotl(carry, 5) ^ mulhiKA(laneInput);
			previous = word;
			word = view.getUint32(offset + 28, true) | 0;
			laneInput = h7 ^ ((word + rotl(previous, 11)) | 0);
			h7 = Math.imul(laneInput, KA);
			carry = rotl(carry, 5) ^ mulhiKA(laneInput);
			previous = word;
			h0 = (h0 + previous) | 0;
			offset += 32;
			remaining -= 32;
		} while (remaining >= 32);

		h0 = Math.imul(h0 ^ rotl(h4, 11), KA) ^ carry;
		h1 = Math.imul(h1 ^ rotl(h5, 19), KB);
		h2 = Math.imul(h2 ^ rotl(h6, 7), KC) ^ rotl(carry, 16);
		h3 = Math.imul(h3 ^ rotl(h7, 23), KD);
	}

	while (remaining >= 16) {
		word = view.getUint32(offset, true) | 0;
		laneInput = h0 ^ ((word + rotl(previous, 11)) | 0);
		h0 = Math.imul(laneInput, KA);
		carry = rotl(carry, 5) ^ mulhiKA(laneInput);
		previous = word;
		word = view.getUint32(offset + 4, true) | 0;
		laneInput = h1 ^ ((word + rotl(previous, 11)) | 0);
		h1 = Math.imul(laneInput, KA);
		carry = rotl(carry, 5) ^ mulhiKA(laneInput);
		previous = word;
		word = view.getUint32(offset + 8, true) | 0;
		laneInput = h2 ^ ((word + rotl(previous, 11)) | 0);
		h2 = Math.imul(laneInput, KA);
		carry = rotl(carry, 5) ^ mulhiKA(laneInput);
		previous = word;
		word = view.getUint32(offset + 12, true) | 0;
		laneInput = h3 ^ ((word + rotl(previous, 11)) | 0);
		h3 = Math.imul(laneInput, KA);
		carry = rotl(carry, 5) ^ mulhiKA(laneInput);
		previous = word;
		offset += 16;
		remaining -= 16;
	}

	h0 = (h0 + rotl(previous, 11)) | 0;

	if (remaining > 8) {
		laneInput = (h0 + inja(view.getUint32(offset, true) | 0)) | 0;
		h0 = Math.imul(laneInput, KA);
		laneInput = (h1 + inja(view.getUint32(offset + 4, true) | 0)) | 0;
		h1 = Math.imul(laneInput, KB);
	}
	if (remaining > 0) {
		laneInput = (h2 + injb(
			view.getUint32(offset + remaining - 8, true) | 0,
		)) | 0;
		h2 = Math.imul(laneInput, KC);
		laneInput = (h3 + injb(
			view.getUint32(offset + remaining - 4, true) | 0,
		)) | 0;
		h3 = Math.imul(laneInput, KD);
	}

	return final4(h0, h1, h2, h3, s0, s1, carry);
}
