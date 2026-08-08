// haya32x64 in pure JavaScript: no BigInt, no wasm, no 64-bit ops.
// State is int32 (V8 smi-friendly); bitwise ops give exact mod-2^32
// semantics, so no `>>> 0` normalization is needed on the hot path.
// The only wide operation is 32x32->64: Math.imul for the low half,
// exact 16-bit-limb integer arithmetic (division-free) for the high
// half. Returns [lo32, hi32] (both unsigned); haya32x64Hex formats.

const KA = 0x9e3779b1 | 0, KB = 0x85ebca77 | 0, KC = 0xc2b2ae3d | 0;
const KD = 0x27d4eb2f | 0, KE = 0x165667b1 | 0;
const F1 = 0x85ebca6b | 0, F2 = 0xc2b2ae35 | 0;
const KAl = 0x9e3779b1 & 0xffff, KAh = 0x9e3779b1 >>> 16;

// Exact high 32 bits of the 64-bit product u * KA (KA fixed), int32
// input, division-free: with X1 = ul*KAh + ((ul*KAl) >>> 16) < 2^32
// and m2 = uh*KAl, hi = uh*KAh + (X1 >>> 16) + (m2 >>> 16) + carry of
// the low-16 sums.
function mulhiKA(u) {
	const ul = u & 0xffff, uh = u >>> 16;
	const X1 = ul * KAh + ((ul * KAl) >>> 16);
	const m2 = uh * KAl;
	return (uh * KAh + (X1 >>> 16) + (m2 >>> 16) +
		((((X1 & 0xffff) + (m2 & 0xffff)) >>> 16))) | 0;
}

// General exact high half for the finalizer/premix (b varies).
function mulhi(a, b) {
	const al = a & 0xffff, ah = a >>> 16;
	const bl = b & 0xffff, bh = b >>> 16;
	const X1 = al * bh + ((al * bl) >>> 16);
	const m2 = ah * bl;
	// X1 < 2^32 but al*bh alone can reach 2^32-ish: al*bh < 2^32,
	// + 2^16 keeps it < 2^32 + 2^16; stay exact via double add then
	// split with unsigned shifts on < 2^32 values only:
	// (al*bh) < (2^16)(2^16) = 2^32 - safe as double; X1 may reach
	// 2^32 + 2^16 - 1, so split it with modF instead of >>>.
	const X1hi = Math.floor(X1 / 65536);
	return (ah * bh + X1hi + (m2 >>> 16) +
		((((X1 - X1hi * 65536) + (m2 & 0xffff)) >>> 16))) | 0;
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

function final4(h0, h1, h2, h3, s0, s1, C, len) {
	h1 = h1 ^ C;
	h3 = h3 ^ rotl(C, 16);
	const lma = (len + KE) | 0;
	const a0 = h0 ^ s0 ^ Math.imul(lma, KA), a1 = h2 ^ KC;
	const b0 = h1 ^ KD, b1 = h3 ^ rotl(s1, 11) ^ mulhiKA(lma);
	let x = Math.imul(a0, a1) ^ mulhi(b0, b1) ^ rotl(h1, 7);
	let y = mulhi(a0, a1) ^ Math.imul(b0, b1) ^ rotl(h0, 19);
	x = x ^ (x >>> 16); x = Math.imul(x, F1);
	y = y ^ (y >>> 15); y = Math.imul(y, F2);
	x = x ^ rotl(y, 13);
	y = y ^ (x >>> 16);
	x = Math.imul(x, KB);
	x = x ^ (x >>> 15);
	y = Math.imul(y, KE);
	y = y ^ (y >>> 14);
	return [x >>> 0, y >>> 0]; // digest = (y << 32) | x
}

// buf: Uint8Array; seedLo/seedHi: u32 halves of the 64-bit seed.
export function haya32x64(buf, seedLo = 0, seedHi = 0) {
	const len = buf.length;
	let l = len;
	let p = 0;

	// Feistel premix: (s0, s1) is a bijection of the 64-bit seed.
	const q1a = seedHi ^ KC;
	const u0 = seedLo ^ Math.imul(q1a, KD) ^ mulhi(q1a, KD);
	const q0a = u0 ^ KA;
	const v0 = seedHi ^ Math.imul(q0a, KB) ^ mulhi(q0a, KB);
	const s0 = u0;
	const s1 = v0;

	if (l <= 8) {
		let a, b;
		if (l >= 4) {
			a = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
			b = buf[l - 4] | (buf[l - 3] << 8) | (buf[l - 2] << 16) |
			    (buf[l - 1] << 24);
		} else if (l > 0) {
			a = buf[0];
			b = (buf[l >> 1] << 8) | (buf[l - 1] << 16);
		} else {
			a = 0; b = 0;
		}
		const xa = inja(a) ^ s0 ^ KB;
		const yb = injb(b) ^ rotl(s1, 13) ^ KE;
		return final4(Math.imul(xa, KA), mulhiKA(xa),
			Math.imul(yb, KC), mulhi(yb, KC), s0, s1, 0, len);
	}

	const view = new DataView(buf.buffer, buf.byteOffset, buf.byteLength);
	let h0 = s0 ^ KA;
	let h1 = (rotl(s1, 7) + KB) | 0;
	let h2 = rotl(s0, 14) ^ KC;
	let h3 = (rotl(s1, 21) + KD) | 0;
	let w = 0, wp = 0, C = 0, u = 0;

	if (l >= 128) {
		let h4 = (s1 + KE) | 0;
		let h5 = rotl(s0, 9) ^ KD;
		let h6 = (rotl(s1, 18) + KA) | 0;
		let h7 = rotl(s0, 27) ^ KB;
		do {
			w = view.getUint32(p, true) | 0;
			u = h0 ^ ((w + rotl(wp, 11)) | 0);
			h0 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
			w = view.getUint32(p + 4, true) | 0;
			u = h1 ^ ((w + rotl(wp, 11)) | 0);
			h1 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
			w = view.getUint32(p + 8, true) | 0;
			u = h2 ^ ((w + rotl(wp, 11)) | 0);
			h2 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
			w = view.getUint32(p + 12, true) | 0;
			u = h3 ^ ((w + rotl(wp, 11)) | 0);
			h3 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
			w = view.getUint32(p + 16, true) | 0;
			u = h4 ^ ((w + rotl(wp, 11)) | 0);
			h4 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
			w = view.getUint32(p + 20, true) | 0;
			u = h5 ^ ((w + rotl(wp, 11)) | 0);
			h5 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
			w = view.getUint32(p + 24, true) | 0;
			u = h6 ^ ((w + rotl(wp, 11)) | 0);
			h6 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
			w = view.getUint32(p + 28, true) | 0;
			u = h7 ^ ((w + rotl(wp, 11)) | 0);
			h7 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
			h0 = (h0 + wp) | 0;
			p += 32; l -= 32;
		} while (l >= 32);
		u = h0 ^ rotl(h4, 11);
		h0 = Math.imul(u, KA) ^ C;
		u = h1 ^ rotl(h5, 19);
		h1 = Math.imul(u, KB);
		u = h2 ^ rotl(h6, 7);
		h2 = Math.imul(u, KC) ^ rotl(C, 16);
		u = h3 ^ rotl(h7, 23);
		h3 = Math.imul(u, KD);
	}

	while (l >= 16) {
		w = view.getUint32(p, true) | 0;
		u = h0 ^ ((w + rotl(wp, 11)) | 0);
		h0 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
		w = view.getUint32(p + 4, true) | 0;
		u = h1 ^ ((w + rotl(wp, 11)) | 0);
		h1 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
		w = view.getUint32(p + 8, true) | 0;
		u = h2 ^ ((w + rotl(wp, 11)) | 0);
		h2 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
		w = view.getUint32(p + 12, true) | 0;
		u = h3 ^ ((w + rotl(wp, 11)) | 0);
		h3 = Math.imul(u, KA); C = rotl(C, 5) ^ mulhiKA(u); wp = w;
		p += 16; l -= 16;
	}

	h0 = (h0 + rotl(wp, 11)) | 0;

	if (l > 8) {
		u = (h0 + inja(view.getUint32(p, true) | 0)) | 0;
		h0 = Math.imul(u, KA);
		u = (h1 + inja(view.getUint32(p + 4, true) | 0)) | 0;
		h1 = Math.imul(u, KB);
	}
	if (l > 0) {
		u = (h2 + injb(view.getUint32(p + l - 8, true) | 0)) | 0;
		h2 = Math.imul(u, KC);
		u = (h3 + injb(view.getUint32(p + l - 4, true) | 0)) | 0;
		h3 = Math.imul(u, KD);
	}

	return final4(h0, h1, h2, h3, s0, s1, C, len);
}

export function haya32x64Hex(buf, seedLo = 0, seedHi = 0) {
	const [lo, hi] = haya32x64(buf, seedLo, seedHi);
	return hi.toString(16).padStart(8, "0") +
	       lo.toString(16).padStart(8, "0");
}
