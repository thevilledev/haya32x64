import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { test } from "node:test";

import {
	digestHex,
	getEngine,
	haya32x64,
	haya32x64Hex,
	haya32x64Pure,
	setWasmModule,
} from "../index.js";

function byteAt(index) {
	return (Math.imul(index, 0x9e3779b1) + 0x7f4a7c15) >>> 24;
}

const key = new Uint8Array(4096);
for (let i = 0; i < key.length; i++) {
	key[i] = byteAt(i);
}

async function vectors() {
	const source = await readFile(
		new URL("../../tests/kat.txt", import.meta.url),
		"utf8",
	);
	return source
		.trim()
		.split("\n")
		.filter((line) => !line.startsWith("#"))
		.map((line) => {
			const [length, seedHi, seedLo, digestHi, digestLo] = line.split(" ");
			return {
				length: Number(length),
				seedHi: Number.parseInt(seedHi, 16),
				seedLo: Number.parseInt(seedLo, 16),
				digestHi: Number.parseInt(digestHi, 16),
				digestLo: Number.parseInt(digestLo, 16),
			};
		});
}

for (const [name, hash] of [
	["hybrid engine", haya32x64],
	["pure JS engine", haya32x64Pure],
]) {
	test(`180 shared C known-answer vectors (${name})`, async () => {
		for (const vector of await vectors()) {
			assert.deepEqual(
				hash(
					key.subarray(0, vector.length),
					vector.seedLo,
					vector.seedHi,
				),
				[vector.digestLo, vector.digestHi],
				`len=${vector.length} seed=${vector.seedHi.toString(16)}${vector.seedLo.toString(16)}`,
			);
		}
	});

	test(`SMHasher3 verification value (${name})`, () => {
		const verificationKey = new Uint8Array(256);
		const hashes = new Uint8Array(256 * 8);
		const view = new DataView(hashes.buffer);
		for (let i = 0; i < 256; i++) {
			const digest = hash(verificationKey.subarray(0, i), 256 - i, 0);
			view.setUint32(i * 8, digest[0], true);
			view.setUint32(i * 8 + 4, digest[1], true);
			verificationKey[i] = i;
		}
		assert.equal(hash(hashes)[0], 0x7137e6dc);
	});
}

test("the embedded wasm reference engine is active", () => {
	assert.equal(getEngine(), "hybrid");
});

test("string input uses UTF-8 and formats in high/low display order", () => {
	const encoded = new TextEncoder().encode("häyähäsh 🚀");
	assert.deepEqual(haya32x64("häyähäsh 🚀"), haya32x64(encoded));
	assert.equal(haya32x64Hex("hello world"), "d3cd28f431c16822");
	assert.equal(digestHex([0x89abcdef, 0x01234567]), "0123456789abcdef");
});

test("subarrays preserve their byte offset", () => {
	const subarray = key.subarray(7, 144);
	const copy = key.slice(7, 144);
	assert.deepEqual(haya32x64(subarray), haya32x64(copy));
	assert.deepEqual(haya32x64Pure(subarray), haya32x64Pure(copy));
});

test("seed words are normalized and invalid inputs are rejected", () => {
	assert.deepEqual(haya32x64("abc", -1, -1), haya32x64("abc", 0xffffffff, 0xffffffff));
	assert.throws(() => haya32x64("abc", 1.5), RangeError);
	assert.throws(() => haya32x64(new Uint16Array(2)), TypeError);
	assert.throws(() => digestHex([1]), TypeError);
});

test("setWasmModule accepts the shipped precompiled module", async () => {
	const bytes = await readFile(
		new URL("../wasm/haya32x64.wasm", import.meta.url),
	);
	setWasmModule(new WebAssembly.Module(bytes));
	assert.equal(getEngine(), "hybrid");
	assert.deepEqual(haya32x64(key), haya32x64Pure(key));
});

test("setWasmModule rejects unrelated modules without replacing the engine", () => {
	const emptyModule = new Uint8Array([
		0x00, 0x61, 0x73, 0x6d,
		0x01, 0x00, 0x00, 0x00,
	]);
	assert.throws(
		() => setWasmModule(new WebAssembly.Module(emptyModule)),
		TypeError,
	);
	assert.equal(getEngine(), "hybrid");
});
