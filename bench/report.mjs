import { readFile } from "node:fs/promises";

if (process.argv.length !== 6) {
	console.error(
		"usage: node bench/report.mjs c8a-js.json c8g-js.json " +
		"c8a-native.txt c8g-native.txt",
	);
	process.exit(2);
}

const [c8aJsPath, c8gJsPath, c8aNativePath, c8gNativePath] =
	process.argv.slice(2);
const [c8aJs, c8gJs, c8aNativeText, c8gNativeText] = await Promise.all([
	readFile(c8aJsPath, "utf8").then(JSON.parse),
	readFile(c8gJsPath, "utf8").then(JSON.parse),
	readFile(c8aNativePath, "utf8"),
	readFile(c8gNativePath, "utf8"),
]);

function value(result, suite, algorithm, length, chunkSize) {
	const match = result.results.find((entry) =>
		entry.suite === suite &&
		entry.algorithm === algorithm &&
		entry.length === length &&
		(chunkSize === undefined || entry.chunkSize === chunkSize));
	if (match === undefined) {
		throw new Error(`missing result: ${suite}/${algorithm}/${length}/${chunkSize}`);
	}
	return match;
}

function fixed(number, digits = 1) {
	return number.toFixed(digits);
}

function table(headers, rows) {
	console.log(`| ${headers.join(" | ")} |`);
	console.log(`|${headers.map(() => "---").join("|")}|`);
	for (const row of rows) console.log(`| ${row.join(" | ")} |`);
	console.log();
}

function nativeResults(text) {
	const results = new Map();
	const sections = [...text.matchAll(/^=== (.+) ===$/gm)];
	for (let index = 0; index < sections.length; index++) {
		const name = sections[index][1];
		if (name === "benchmark metadata") continue;
		const start = sections[index].index + sections[index][0].length;
		const end = sections[index + 1]?.index ?? text.length;
		const section = text.slice(start, end);
		const average = section.match(/Average\s+-\s+([\d.]+) cycles\/hash/);
		const bulk = section.match(
			/Bulk speed test - 262144-byte keys[\s\S]*?^Average\s+-\s+([\d.]+) bytes\/cycle\s+-\s+([\d.]+) GiB\/sec/m,
		);
		if (average !== null && bulk !== null) {
			results.set(name, {
				averageCycles: Number(average[1]),
				bytesPerCycle: Number(bulk[1]),
				gibPerSecond: Number(bulk[2]),
			});
		}
	}
	return results;
}

console.log("## JavaScript byte API: small keys (median ns/hash)\n");
const byteAlgorithms = [
	["haya32x64 pure JS", 64],
	["haya32x64 hybrid", 64],
	["hayahash64 BigInt", 64],
	["hayahash64 wasm", 64],
	["xxhash-wasm XXH64", 64],
	["xxhashjs XXH64", 64],
	["cyrb53 bytes", 53],
];
table(
	["algorithm", "bits", "C8a 4 B", "C8a 8 B", "C8g 4 B", "C8g 8 B"],
	byteAlgorithms.map(([algorithm, width]) => [
		algorithm,
		width,
		fixed(value(c8aJs, "bytes-one-shot", algorithm, 4).medianNs),
		fixed(value(c8aJs, "bytes-one-shot", algorithm, 8).medianNs),
		fixed(value(c8gJs, "bytes-one-shot", algorithm, 4).medianNs),
		fixed(value(c8gJs, "bytes-one-shot", algorithm, 8).medianNs),
	]),
);

console.log("## JavaScript byte API: 1 MiB (median MB/s)\n");
table(
	["algorithm", "bits", "C8a", "C8g"],
	byteAlgorithms.map(([algorithm, width]) => [
		algorithm,
		width,
		fixed(value(c8aJs, "bytes-one-shot", algorithm, 1048576).mbPerSecond, 0),
		fixed(value(c8gJs, "bytes-one-shot", algorithm, 1048576).mbPerSecond, 0),
	]),
);

console.log("## haya32x64 JavaScript/wasm crossover (median ns/hash)\n");
table(
	["engine", "host", "15 B", "16 B", "17 B", "4 KiB"],
	[
		["pure JS", "C8a", c8aJs, "haya32x64 pure JS"],
		["hybrid", "C8a", c8aJs, "haya32x64 hybrid"],
		["pure JS", "C8g", c8gJs, "haya32x64 pure JS"],
		["hybrid", "C8g", c8gJs, "haya32x64 hybrid"],
	].map(([engine, host, result, algorithm]) => [
		engine,
		host,
		...([15, 16, 17, 4096].map((length) =>
			fixed(value(result, "bytes-one-shot", algorithm, length).medianNs))),
	]),
);

console.log("## Streaming 1 MiB byte input (median MB/s)\n");
const streamAlgorithms = [
	["haya32x64 pure JS", 64],
	["xxhash-wasm XXH64", 64],
	["xxhashjs XXH64", 64],
];
table(
	["algorithm", "chunk", "C8a", "C8g"],
	streamAlgorithms.flatMap(([algorithm]) => [64, 4096, 65536].map((chunk) => [
		algorithm,
		`${chunk} B`,
		fixed(value(c8aJs, "bytes-streaming", algorithm, 1048576, chunk).mbPerSecond, 0),
		fixed(value(c8gJs, "bytes-streaming", algorithm, 1048576, chunk).mbPerSecond, 0),
	])),
);

console.log("## ASCII string API (median ns/hash)\n");
const stringAlgorithms = [
	["haya32x64 pure JS", 64],
	["haya32x64 hybrid", 64],
	["hayahash64 BigInt", 64],
	["hayahash64 wasm", 64],
	["xxhash-wasm XXH64", 64],
	["xxhashjs XXH64", 64],
	["cyrb53 string", 53],
	["imurmurhash", 32],
];
table(
	["algorithm", "bits", "C8a 8 chars", "C8a 32 chars", "C8g 8 chars", "C8g 32 chars"],
	stringAlgorithms.map(([algorithm, width]) => [
		algorithm,
		width,
		fixed(value(c8aJs, "ascii-string-one-shot", algorithm, 8).medianNs),
		fixed(value(c8aJs, "ascii-string-one-shot", algorithm, 32).medianNs),
		fixed(value(c8gJs, "ascii-string-one-shot", algorithm, 8).medianNs),
		fixed(value(c8gJs, "ascii-string-one-shot", algorithm, 32).medianNs),
	]),
);

console.log("## Native SMHasher3 speed\n");
const c8aNative = nativeResults(c8aNativeText);
const c8gNative = nativeResults(c8gNativeText);
const nativeOrder = [
	["haya32x64", 64],
	["khashv-64", 64],
	["lookup3", 64],
	["MurmurHash2-64.int32", 64],
	["a5hash-32", 32],
	["XXH-32", 32],
	["t1ha0", 64],
	["a5hash-128.64", 64],
	["XXH3-64", 64],
	["rapidhash", 64],
	["wyhash.strict", 64],
	["ChibiHash2", 64],
	["MuseAir.bfast", 64],
];
table(
	["algorithm", "bits", "C8a cycles 1–31 B", "C8a B/cycle", "C8g cycles 1–31 B", "C8g B/cycle"],
	nativeOrder.map(([algorithm, width]) => {
		const a = c8aNative.get(algorithm);
		const g = c8gNative.get(algorithm);
		return [
			algorithm,
			width,
			a === undefined ? "—" : fixed(a.averageCycles, 2),
			a === undefined ? "—" : fixed(a.bytesPerCycle, 2),
			g === undefined ? "—" : fixed(g.averageCycles, 2),
			g === undefined ? "—" : fixed(g.bytesPerCycle, 2),
		];
	}),
);
