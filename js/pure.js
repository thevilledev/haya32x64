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

function final4(h0, h1, h2, h3, s0, s1, carry, length) {
	h1 ^= carry;
	h3 ^= rotl(carry, 16);
	const lengthInput = (length + KE) | 0;
	const lengthLo = Math.imul(lengthInput, KA);
	const lengthHi = mulhiKA(lengthInput);
	const a0 = h0 ^ s0 ^ lengthLo;
	const a1 = h2 ^ KC;
	const b0 = h1 ^ KD;
	const b1 = h3 ^ rotl(s1, 11) ^ lengthHi;
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

// Keep the no-stripe 9..15-byte path separate from the much larger block
// engine. V8 otherwise changes its tiering and register allocation for this
// path when the bulk loop evolves, despite the bulk branch never being taken.
function hash9to15(bytes, s0, s1, length) {
	const view = new DataView(
		bytes.buffer,
		bytes.byteOffset,
		bytes.byteLength,
	);
	let h0 = s0 ^ KA;
	let h1 = (rotl(s1, 7) + KB) | 0;
	let h2 = rotl(s0, 14) ^ KC;
	let h3 = (rotl(s1, 21) + KD) | 0;
	h0 = Math.imul(
		(h0 + inja(view.getUint32(0, true) | 0)) | 0,
		KA,
	);
	h1 = Math.imul(
		(h1 + inja(view.getUint32(4, true) | 0)) | 0,
		KB,
	);
	h2 = Math.imul(
		(h2 + injb(view.getUint32(length - 8, true) | 0)) | 0,
		KC,
	);
	h3 = Math.imul(
		(h3 + injb(view.getUint32(length - 4, true) | 0)) | 0,
		KD,
	);
	return final4(h0, h1, h2, h3, s0, s1, 0, length);
}

/**
 * Hashes bytes using the pure-JavaScript engine.
 * @param {Uint8Array} bytes
 * @param {number} seedLo
 * @param {number} seedHi
 * @returns {[number, number]} digest words in [low, high] order
 */
export function hashPure(bytes, seedLo = 0, seedHi = 0) {
	const total = bytes.length;
	let remaining = total;
	let offset = 0;

	// Two-round Feistel seed premix; bijective in (seedLo, seedHi).
	const q1a = seedHi ^ KC;
	const u0 = seedLo ^ Math.imul(q1a, KD) ^ mulhi(q1a, KD);
	const q0a = u0 ^ KA;
	const v0 = seedHi ^ Math.imul(q0a, KB) ^ mulhi(q0a, KB);
	const s0 = u0;
	const s1 = v0;

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
			s0, s1, 0, total,
		);
	}
	if (remaining < 16) {
		return hash9to15(bytes, s0, s1, total);
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
	let previous = 0;
	let carry = 0;
	let laneInput = 0;

	if (remaining >= 128) {
		const bulk = {
			h: [
				h0,
				h1,
				h2,
				h3,
				(s1 + KE) | 0,
				rotl(s0, 9) ^ KD,
				(rotl(s1, 18) + KA) | 0,
				rotl(s0, 27) ^ KB,
			],
			previous,
			carry,
		};
		const blocks = Math.floor(remaining / 32) * 32;
		streamBlocks(bulk, bytes, offset, blocks);
		[h0, h1, h2, h3] = bulk.h;
		const h4 = bulk.h[4];
		const h5 = bulk.h[5];
		const h6 = bulk.h[6];
		const h7 = bulk.h[7];
		previous = bulk.previous;
		carry = bulk.carry;
		offset += blocks;
		remaining -= blocks;

		h0 = Math.imul(h0 ^ rotl(h5, 11), KA) ^ carry;
		h1 = Math.imul(h1 ^ rotl(h6, 19), KB);
		h2 = Math.imul(h2 ^ rotl(h7, 7), KC) ^ rotl(carry, 16);
		h3 = Math.imul(h3 ^ rotl(h4, 23), KD);
	}

	while (remaining >= 16) {
		const w0 = view.getUint32(offset, true) | 0;
		const w1 = view.getUint32(offset + 4, true) | 0;
		const w2 = view.getUint32(offset + 8, true) | 0;
		const w3 = view.getUint32(offset + 12, true) | 0;
		const lane0 = h0 ^ ((w0 + rotl(previous, 11)) | 0);
		const lane1 = h1 ^ ((w1 + rotl(w0, 11)) | 0);
		const lane2 = h2 ^ ((w2 + rotl(w1, 11)) | 0);
		const lane3 = h3 ^ ((w3 + rotl(w2, 11)) | 0);
		const high0 = mulhiKA(lane0);
		const high1 = mulhiKA(lane1);
		const high2 = mulhiKA(lane2);
		const high3 = mulhiKA(lane3);
		h0 = Math.imul(lane0, KA);
		h1 = Math.imul(lane1, KA);
		h2 = Math.imul(lane2, KA);
		h3 = Math.imul(lane3, KA);
		carry = rotl(carry, 20) ^ rotl(high0, 15) ^
			rotl(high1, 10) ^ rotl(high2, 5) ^ high3;
		previous = w3;
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

	return final4(h0, h1, h2, h3, s0, s1, carry, total);
}

const STREAM_KEEP = 64;
const STREAM_CAPACITY = 192;

function streamBlocks(state, bytes, offset, length) {
	const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
	let h0 = state.h[0];
	let h1 = state.h[1];
	let h2 = state.h[2];
	let h3 = state.h[3];
	let h4 = state.h[4];
	let h5 = state.h[5];
	let h6 = state.h[6];
	let h7 = state.h[7];
	let previous = state.previous;
	const end = offset + length;
	// Pair-lane bulk kernel: pair i is (h[i], h[i+4]); one full 32x32
	// product absorbs eight bytes.  The carry accumulator is untouched
	// here; it stays zero until the 16-byte remainder stripes.
	while (offset !== end) {
		const u0 = (view.getUint32(offset, true) | 0) ^ KA;
		const u1 = (view.getUint32(offset + 4, true) | 0) ^ KB;
		const u2 = (view.getUint32(offset + 8, true) | 0) ^ KB;
		const u3 = (view.getUint32(offset + 12, true) | 0) ^ KC;
		const u4 = (view.getUint32(offset + 16, true) | 0) ^ KC;
		const u5 = (view.getUint32(offset + 20, true) | 0) ^ KD;
		const u6 = (view.getUint32(offset + 24, true) | 0) ^ KD;
		const w7 = view.getUint32(offset + 28, true) | 0;
		const u7 = w7 ^ KE;
		const x0 = (u0 + h0) | 0;
		const y0 = (u1 + h4) | 0;
		h0 = (Math.imul(x0, y0) + rotl(u1, 16)) | 0;
		h4 = mulhi(x0, y0) ^ u0;
		const x1 = (u2 + h1) | 0;
		const y1 = (u3 + h5) | 0;
		h1 = (Math.imul(x1, y1) + rotl(u3, 16)) | 0;
		h5 = mulhi(x1, y1) ^ u2;
		const x2 = (u4 + h2) | 0;
		const y2 = (u5 + h6) | 0;
		h2 = (Math.imul(x2, y2) + rotl(u5, 16)) | 0;
		h6 = mulhi(x2, y2) ^ u4;
		const x3 = (u6 + h3) | 0;
		const y3 = (u7 + h7) | 0;
		h3 = (Math.imul(x3, y3) + rotl(u7, 16)) | 0;
		h7 = mulhi(x3, y3) ^ u6;
		previous = w7;
		offset += 32;
	}
	state.h[0] = h0;
	state.h[1] = h1;
	state.h[2] = h2;
	state.h[3] = h3;
	state.h[4] = h4;
	state.h[5] = h5;
	state.h[6] = h6;
	state.h[7] = h7;
	state.previous = previous;
}

/** Bit-exact, unknown-length streaming state for the pure-JavaScript engine. */
export class Haya32x64PureStream {
	constructor(seedLo = 0, seedHi = 0) {
		if (!Number.isInteger(seedLo) || !Number.isInteger(seedHi)) {
			throw new RangeError("seed words must be integers");
		}
		this.seedLo = seedLo >>> 0;
		this.seedHi = seedHi >>> 0;
		const q1a = (this.seedHi | 0) ^ KC;
		const s0 = (this.seedLo | 0) ^ Math.imul(q1a, KD) ^ mulhi(q1a, KD);
		const q0a = s0 ^ KA;
		const s1 = (this.seedHi | 0) ^ Math.imul(q0a, KB) ^ mulhi(q0a, KB);
		this.h = [
			s0 ^ KA,
			(rotl(s1, 7) + KB) | 0,
			rotl(s0, 14) ^ KC,
			(rotl(s1, 21) + KD) | 0,
			(s1 + KE) | 0,
			rotl(s0, 9) ^ KD,
			(rotl(s1, 18) + KA) | 0,
			rotl(s0, 27) ^ KB,
		];
		this.s0 = s0;
		this.s1 = s1;
		this.previous = 0;
		this.carry = 0;
		this.total = 0;
		this.buffered = 0;
		this.bulk = false;
		this.buffer = new Uint8Array(STREAM_CAPACITY);
	}

	update(bytes) {
		if (!(bytes instanceof Uint8Array)) {
			throw new TypeError("stream input must be a Uint8Array");
		}
		if (bytes.length > 0xffffffff - this.total) {
			throw new RangeError("stream length must be at most 0xffffffff bytes");
		}
		if (bytes.length === 0) {
			return this;
		}
		this.total += bytes.length;
		let offset = 0;
		let remaining = bytes.length;

		if (!this.bulk) {
			const room = STREAM_CAPACITY - this.buffered;
			if (remaining < room) {
				this.buffer.set(bytes, this.buffered);
				this.buffered += remaining;
				return this;
			}
			this.bulk = true;
		}

		for (;;) {
			if (this.buffered === STREAM_KEEP && remaining > STREAM_CAPACITY) {
				const direct = Math.floor(
					(remaining - STREAM_KEEP) / 32,
				) * 32;
				streamBlocks(this, this.buffer, 0, STREAM_KEEP);
				streamBlocks(this, bytes, offset, direct);
				offset += direct;
				remaining -= direct;
				this.buffered = 0;
			}

			const room = STREAM_CAPACITY - this.buffered;
			const take = Math.min(remaining, room);
			this.buffer.set(bytes.subarray(offset, offset + take), this.buffered);
			this.buffered += take;
			offset += take;
			remaining -= take;
			if (this.buffered < STREAM_CAPACITY) {
				break;
			}

			const consume = (this.buffered - STREAM_KEEP) & ~31;
			streamBlocks(this, this.buffer, 0, consume);
			this.buffer.copyWithin(0, consume, this.buffered);
			this.buffered -= consume;
		}
		return this;
	}

	digest() {
		if (!this.bulk) {
			return hashPure(
				this.buffer.subarray(0, this.total),
				this.seedLo,
				this.seedHi,
			);
		}

		const tail = {
			h: this.h.slice(),
			previous: this.previous,
			carry: this.carry,
		};
		const blocks = this.buffered & ~31;
		streamBlocks(tail, this.buffer, 0, blocks);
		let offset = blocks;
		let remaining = this.buffered - blocks;
		let h0 = tail.h[0];
		let h1 = tail.h[1];
		let h2 = tail.h[2];
		let h3 = tail.h[3];
		let previous = tail.previous;
		let carry = tail.carry;

		h0 = Math.imul(h0 ^ rotl(tail.h[5], 11), KA) ^ carry;
		h1 = Math.imul(h1 ^ rotl(tail.h[6], 19), KB);
		h2 = Math.imul(h2 ^ rotl(tail.h[7], 7), KC) ^ rotl(carry, 16);
		h3 = Math.imul(h3 ^ rotl(tail.h[4], 23), KD);

		const view = new DataView(this.buffer.buffer);
		let word;
		let laneInput;
		if (remaining >= 16) {
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
			h0 = Math.imul(
				(h0 + inja(view.getUint32(offset, true) | 0)) | 0,
				KA,
			);
			h1 = Math.imul(
				(h1 + inja(view.getUint32(offset + 4, true) | 0)) | 0,
				KB,
			);
		}
		if (remaining > 0) {
			h2 = Math.imul(
				(h2 + injb(view.getUint32(offset + remaining - 8, true) | 0)) | 0,
				KC,
			);
			h3 = Math.imul(
				(h3 + injb(view.getUint32(offset + remaining - 4, true) | 0)) | 0,
				KD,
			);
		}
		return final4(
			h0, h1, h2, h3, this.s0, this.s1, carry, this.total,
		);
	}
}
