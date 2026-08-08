// Public JavaScript API.  Small keys stay in JavaScript to avoid a wasm
// boundary; larger keys use the reference C header compiled to wasm when the
// runtime permits it.  Both engines are bit-exact and neither uses BigInt.

import { hashPure } from "./pure.js";
import { initWasm, initWasmFromModule } from "./wasm.js";

const WASM_MIN_LENGTH = 16;
const WASM_MAX_LENGTH = 0xffffffff;
const encoder = new TextEncoder();
let wasm = initWasm();

function normalizeWord(value, name) {
	if (!Number.isInteger(value)) {
		throw new RangeError(`${name} must be an integer`);
	}
	return value >>> 0;
}

function normalizeInput(input) {
	let data;
	if (typeof input === "string") {
		data = encoder.encode(input);
	} else if (input instanceof Uint8Array) {
		data = input;
	} else {
		throw new TypeError("input must be a Uint8Array or string");
	}
	if (data.length > WASM_MAX_LENGTH) {
		throw new RangeError("input must be at most 0xffffffff bytes");
	}
	return data;
}

/** Returns whether the embedded WebAssembly reference engine is available. */
export function getEngine() {
	return wasm === null ? "js" : "hybrid";
}

/** Activates a precompiled copy of the shipped `haya32x64.wasm` module. */
export function setWasmModule(module) {
	wasm = initWasmFromModule(module);
}

/**
 * Hashes bytes (or a UTF-8 string) and returns `[low32, high32]`.
 * The 64-bit seed is likewise passed as two words, low first.
 */
export function haya32x64(input, seedLo = 0, seedHi = 0) {
	const data = normalizeInput(input);
	const lo = normalizeWord(seedLo, "seedLo");
	const hi = normalizeWord(seedHi, "seedHi");
	if (
		wasm !== null &&
		data.length >= WASM_MIN_LENGTH &&
		data.length <= WASM_MAX_LENGTH
	) {
		return wasm.hash(data, lo, hi);
	}
	return hashPure(data, lo, hi);
}

/** Same digest as `haya32x64`, forced through the pure-JavaScript engine. */
export function haya32x64Pure(input, seedLo = 0, seedHi = 0) {
	return hashPure(
		normalizeInput(input),
		normalizeWord(seedLo, "seedLo"),
		normalizeWord(seedHi, "seedHi"),
	);
}

/** Formats a `[low32, high32]` digest as 16 lowercase hexadecimal digits. */
export function digestHex(digest) {
	if (
		!Array.isArray(digest) ||
		digest.length !== 2 ||
		!Number.isInteger(digest[0]) ||
		!Number.isInteger(digest[1])
	) {
		throw new TypeError("digest must be a two-word array");
	}
	return (digest[1] >>> 0).toString(16).padStart(8, "0") +
		(digest[0] >>> 0).toString(16).padStart(8, "0");
}

/** Hashes and returns the 64-bit digest as 16 hexadecimal digits. */
export function haya32x64Hex(input, seedLo = 0, seedHi = 0) {
	return digestHex(haya32x64(input, seedLo, seedHi));
}
