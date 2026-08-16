import {
	createHaya32x64,
	digestHex,
	getEngine,
	haya32x64,
	haya32x64Hex,
	haya32x64Pure,
	setWasmModule,
	type Haya32x64Digest,
	type Haya32x64Input,
} from "./index.js";
import { Haya32x64PureStream, hashPure } from "./pure.js";

const input: Haya32x64Input = "hello";
const bytes: Haya32x64Input = new Uint8Array(0);
const digest: Haya32x64Digest = haya32x64(input, 1, 2);
const hex: string = haya32x64Hex(bytes);
const engine: "hybrid" | "js" = getEngine();
const streamed: Haya32x64Digest = createHaya32x64()
	.update(input)
	.update(bytes)
	.digest();
const formatted: string = digestHex(digest);
const setModule: typeof setWasmModule = setWasmModule;
const pure: Haya32x64Digest = haya32x64Pure(input);
const raw: Haya32x64Digest = hashPure(new Uint8Array(4), 0, 0);
const pureStream: Haya32x64Digest = new Haya32x64PureStream(0, 0)
	.update(new Uint8Array(1))
	.digest();

void engine;
void hex;
void streamed;
void formatted;
void setModule;
void pure;
void raw;
void pureStream;
