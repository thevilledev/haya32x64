export type Haya32x64Input = Uint8Array | string;
export type Haya32x64Digest = [low32: number, high32: number];

export declare function getEngine(): "hybrid" | "js";
export declare function setWasmModule(module: WebAssembly.Module): void;
export declare function haya32x64(
	input: Haya32x64Input,
	seedLo?: number,
	seedHi?: number,
): Haya32x64Digest;
export declare function haya32x64Pure(
	input: Haya32x64Input,
	seedLo?: number,
	seedHi?: number,
): Haya32x64Digest;
export declare class Haya32x64Stream {
	constructor(seedLo?: number, seedHi?: number);
	update(input: Haya32x64Input): this;
	digest(): Haya32x64Digest;
	digestHex(): string;
}
export declare function createHaya32x64(
	seedLo?: number,
	seedHi?: number,
): Haya32x64Stream;
export declare function digestHex(digest: Haya32x64Digest): string;
export declare function haya32x64Hex(
	input: Haya32x64Input,
	seedLo?: number,
	seedHi?: number,
): string;
