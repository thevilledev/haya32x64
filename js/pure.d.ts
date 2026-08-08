import type { Haya32x64Digest } from "./index.js";

export declare function hashPure(
	bytes: Uint8Array,
	seedLo?: number,
	seedHi?: number,
): Haya32x64Digest;
