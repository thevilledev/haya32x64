import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { test } from "node:test";

import { haya32x64, haya32x64Pure } from "../index.js";

const corpusPath = process.env.HAYA32X64_CORPUS;

test(
	"randomized C-reference differential corpus",
	{ skip: corpusPath === undefined ? "HAYA32X64_CORPUS is unset" : false },
	async () => {
		const file = await readFile(corpusPath);
		const corpus = new Uint8Array(file.buffer, file.byteOffset, file.byteLength);
		const view = new DataView(corpus.buffer, corpus.byteOffset, corpus.byteLength);
		let offset = 0;
		const take = (length) => {
			const start = offset;
			offset += length;
			assert.ok(offset <= corpus.length, `truncated corpus at ${start}`);
			return corpus.subarray(start, offset);
		};
		const read32 = () => {
			const start = offset;
			take(4);
			return view.getUint32(start, true);
		};

		assert.equal(new TextDecoder().decode(take(8)), "H32XFZ01");
		const caseCount = read32();
		const corpusSeedLo = read32();
		const corpusSeedHi = read32();
		const casesOffset = offset;
		for (const [engine, hash] of [
			["hybrid", haya32x64],
			["pure JS", haya32x64Pure],
		]) {
			offset = casesOffset;
			for (let i = 0; i < caseCount; i++) {
				const length = read32();
				const seedLo = read32();
				const seedHi = read32();
				const digestLo = read32();
				const digestHi = read32();
				assert.deepEqual(
					hash(take(length), seedLo, seedHi),
					[digestLo, digestHi],
					`${engine} case=${i} len=${length}`,
				);
			}
			assert.equal(offset, corpus.length, "trailing bytes in corpus");
		}
		console.error(
			`both JS engines matched ${caseCount} C cases ` +
			`(seed=${corpusSeedHi.toString(16).padStart(8, "0")}` +
			`${corpusSeedLo.toString(16).padStart(8, "0")})`,
		);
	},
);
