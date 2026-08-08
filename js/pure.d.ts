import type { Haya32x64Digest } from "./index.js";

export declare function hashPure(
	bytes: Uint8Array,
	seedLo?: number,
	seedHi?: number,
): Haya32x64Digest;

export declare class Haya32x64PureStream {
	constructor(seedLo?: number, seedHi?: number);
	update(bytes: Uint8Array): this;
	digest(): Haya32x64Digest;
}
