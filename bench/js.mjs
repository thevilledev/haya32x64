import { execFileSync } from "node:child_process";
import os from "node:os";

import { hayahash64, hayahash64Pure } from "hayahash";
import ImurmurHash from "imurmurhash";
import initXxhashWasm from "xxhash-wasm";
import XXH from "xxhashjs";

import {
	createHaya32x64,
	getEngine,
	haya32x64,
	haya32x64Pure,
} from "../js/index.js";

const SAMPLE_MS = numberFromEnvironment("HAYA_BENCH_SAMPLE_MS", 150);
const SAMPLE_COUNT = numberFromEnvironment("HAYA_BENCH_SAMPLES", 9);
const WARMUP_MS = numberFromEnvironment("HAYA_BENCH_WARMUP_MS", 100);
const ALGORITHM_FILTER = process.env.HAYA_BENCH_FILTER;
const BYTE_LENGTHS = [1, 4, 8, 15, 16, 17, 32, 64, 256, 4096, 1048576];
const STRING_LENGTHS = [8, 32, 256, 4096];
const STREAM_LENGTH = 1048576;
const STREAM_CHUNKS = [64, 4096, 65536];

let sink = 0;

function numberFromEnvironment(name, fallback) {
	const raw = process.env[name];
	if (raw === undefined) {
		return fallback;
	}
	const value = Number(raw);
	if (!Number.isFinite(value) || value <= 0) {
		throw new RangeError(`${name} must be a positive number`);
	}
	return value;
}

function fillBytes(length) {
	// Buffer is a Uint8Array subclass accepted directly by every byte API in
	// this matrix, including xxhashjs's Node-specific implementation.
	const bytes = Buffer.allocUnsafe(length);
	let state = 0x9e3779b9;
	for (let index = 0; index < length; index++) {
		state ^= state << 13;
		state ^= state >>> 17;
		state ^= state << 5;
		bytes[index] = state;
	}
	return bytes;
}

function fillString(length) {
	let value = "";
	for (let index = 0; index < length; index++) {
		value += String.fromCharCode(32 + ((index * 29 + 17) % 95));
	}
	return value;
}

// cyrb53 by bryc, adapted to consume bytes directly for the byte-input lane.
// The string spelling below is the original public-domain algorithm.
function cyrb53Bytes(bytes, seed = 0) {
	let h1 = 0xdeadbeef ^ seed;
	let h2 = 0x41c6ce57 ^ seed;
	for (let index = 0; index < bytes.length; index++) {
		const byte = bytes[index];
		h1 = Math.imul(h1 ^ byte, 2654435761);
		h2 = Math.imul(h2 ^ byte, 1597334677);
	}
	h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507) ^
		Math.imul(h2 ^ (h2 >>> 13), 3266489909);
	h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507) ^
		Math.imul(h1 ^ (h1 >>> 13), 3266489909);
	return 4294967296 * (2097151 & h2) + (h1 >>> 0);
}

function cyrb53String(value, seed = 0) {
	let h1 = 0xdeadbeef ^ seed;
	let h2 = 0x41c6ce57 ^ seed;
	for (let index = 0; index < value.length; index++) {
		const code = value.charCodeAt(index);
		h1 = Math.imul(h1 ^ code, 2654435761);
		h2 = Math.imul(h2 ^ code, 1597334677);
	}
	h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507) ^
		Math.imul(h2 ^ (h2 >>> 13), 3266489909);
	h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507) ^
		Math.imul(h1 ^ (h1 >>> 13), 3266489909);
	return 4294967296 * (2097151 & h2) + (h1 >>> 0);
}

function lowWord(value) {
	if (Array.isArray(value)) {
		return value[0] | 0;
	}
	if (typeof value === "bigint") {
		return Number(BigInt.asIntN(32, value));
	}
	if (typeof value === "number") {
		return value | 0;
	}
	if (value !== null && typeof value.toNumber === "function") {
		return value.toNumber() | 0;
	}
	throw new TypeError(`unsupported digest type: ${typeof value}`);
}

function runBatch(operation, iterations) {
	let accumulator = 0;
	for (let index = 0; index < iterations; index++) {
		accumulator = (accumulator + lowWord(operation())) | 0;
	}
	sink ^= accumulator;
}

function elapsedMilliseconds(operation, iterations) {
	const started = performance.now();
	runBatch(operation, iterations);
	return performance.now() - started;
}

function percentile(sorted, fraction) {
	return sorted[Math.round((sorted.length - 1) * fraction)];
}

function measure(operation) {
	let iterations = 1;
	let elapsed = elapsedMilliseconds(operation, iterations);
	while (elapsed < 20 && iterations < 0x40000000) {
		iterations *= 2;
		elapsed = elapsedMilliseconds(operation, iterations);
	}
	iterations = Math.max(1, Math.round(iterations * SAMPLE_MS / elapsed));

	const warmupDeadline = performance.now() + WARMUP_MS;
	while (performance.now() < warmupDeadline) {
		runBatch(operation, iterations);
	}

	const samples = [];
	for (let sample = 0; sample < SAMPLE_COUNT; sample++) {
		const milliseconds = elapsedMilliseconds(operation, iterations);
		samples.push(milliseconds * 1e6 / iterations);
	}
	samples.sort((left, right) => left - right);
	return {
		iterations,
		medianNs: percentile(samples, 0.5),
		p10Ns: percentile(samples, 0.1),
		p90Ns: percentile(samples, 0.9),
	};
}

function repositoryRevision() {
	try {
		return execFileSync("git", ["rev-parse", "HEAD"], {
			cwd: new URL("..", import.meta.url),
			encoding: "utf8",
			stdio: ["ignore", "pipe", "ignore"],
		}).trim();
	} catch {
		return "unknown";
	}
}

function repositoryDirty() {
	try {
		return execFileSync("git", ["status", "--porcelain", "--untracked-files=no"], {
			cwd: new URL("..", import.meta.url),
			encoding: "utf8",
			stdio: ["ignore", "pipe", "ignore"],
		}).trim() !== "";
	} catch {
		return null;
	}
}

function splitBytes(bytes, chunkSize) {
	const chunks = [];
	for (let offset = 0; offset < bytes.length; offset += chunkSize) {
		chunks.push(bytes.subarray(offset, Math.min(offset + chunkSize, bytes.length)));
	}
	return chunks;
}

function record(results, suite, algorithm, width, length, operation, extra = {}) {
	if (ALGORITHM_FILTER !== undefined &&
		!algorithm.includes(ALGORITHM_FILTER)) {
		return;
	}
	if (globalThis.gc !== undefined) {
		globalThis.gc();
	}
	process.stderr.write(`${suite}: ${algorithm} length=${length}` +
		(extra.chunkSize === undefined ? "\n" : ` chunk=${extra.chunkSize}\n`));
	const measured = measure(operation);
	results.push({
		suite,
		algorithm,
		width,
		length,
		...extra,
		...measured,
		mbPerSecond: length * 1000 / measured.medianNs,
	});
}

const xxhashWasm = await initXxhashWasm();
const results = [];

const byteAlgorithms = [
	["haya32x64 pure JS", 64, (input) => haya32x64Pure(input)],
	["haya32x64 hybrid", 64, (input) => haya32x64(input)],
	["hayahash64 BigInt", 64, (input) => hayahash64Pure(input)],
	["hayahash64 wasm", 64, (input) => hayahash64(input)],
	["xxhash-wasm XXH64", 64, (input) => xxhashWasm.h64Raw(input)],
	["xxhashjs XXH64", 64, (input) => XXH.h64(input, 0)],
	["cyrb53 bytes", 53, (input) => cyrb53Bytes(input)],
];

for (const length of BYTE_LENGTHS) {
	const input = fillBytes(length);
	for (const [algorithm, width, hash] of byteAlgorithms) {
		record(results, "bytes-one-shot", algorithm, width, length, () => hash(input));
	}
}

const stringAlgorithms = [
	["haya32x64 pure JS", 64, (input) => haya32x64Pure(input)],
	["haya32x64 hybrid", 64, (input) => haya32x64(input)],
	["hayahash64 BigInt", 64, (input) => hayahash64Pure(input)],
	["hayahash64 wasm", 64, (input) => hayahash64(input)],
	["xxhash-wasm XXH64", 64, (input) => xxhashWasm.h64(input)],
	["xxhashjs XXH64", 64, (input) => XXH.h64(input, 0)],
	["cyrb53 string", 53, (input) => cyrb53String(input)],
	["imurmurhash", 32, (input) => ImurmurHash(input).result()],
];

for (const length of STRING_LENGTHS) {
	const input = fillString(length);
	for (const [algorithm, width, hash] of stringAlgorithms) {
		record(results, "ascii-string-one-shot", algorithm, width, length,
			() => hash(input));
	}
}

const streamInput = fillBytes(STREAM_LENGTH);
for (const chunkSize of STREAM_CHUNKS) {
	const chunks = splitBytes(streamInput, chunkSize);
	const streamAlgorithms = [
		["haya32x64 pure JS", 64, () => {
			const state = createHaya32x64();
			for (const chunk of chunks) state.update(chunk);
			return state.digest();
		}],
		["xxhash-wasm XXH64", 64, () => {
			const state = xxhashWasm.create64();
			for (const chunk of chunks) state.update(chunk);
			return state.digest();
		}],
		["xxhashjs XXH64", 64, () => {
			const state = XXH.h64(0);
			for (const chunk of chunks) state.update(chunk);
			return state.digest();
		}],
	];
	for (const [algorithm, width, hash] of streamAlgorithms) {
		record(results, "bytes-streaming", algorithm, width, STREAM_LENGTH,
			hash, { chunkSize });
	}
}

const output = {
	schema: 1,
	createdAt: new Date().toISOString(),
	revision: repositoryRevision(),
	dirty: repositoryDirty(),
	platform: {
		arch: process.arch,
		platform: process.platform,
		release: os.release(),
		cpu: os.cpus()[0]?.model ?? "unknown",
		logicalCpus: os.cpus().length,
	},
	runtime: process.versions,
	engine: getEngine(),
	config: {
		sampleMs: SAMPLE_MS,
		samples: SAMPLE_COUNT,
		warmupMs: WARMUP_MS,
		algorithmFilter: ALGORITHM_FILTER ?? null,
		byteLengths: BYTE_LENGTHS,
		stringLengths: STRING_LENGTHS,
		streamLength: STREAM_LENGTH,
		streamChunks: STREAM_CHUNKS,
	},
	results,
	sink,
};

process.stdout.write(`${JSON.stringify(output, null, 2)}\n`);
