// Wrapper for the reference C header compiled to wasm32.  The export accepts
// and returns split u32 words, so even the WebAssembly boundary uses no BigInt.

import { wasmBase64 } from "./wasm-module.js";

const PAGE_SIZE = 65536;

function decodeBase64(base64) {
	const binary = atob(base64);
	const bytes = new Uint8Array(binary.length);
	for (let i = 0; i < binary.length; i++) {
		bytes[i] = binary.charCodeAt(i);
	}
	return bytes;
}

export function initWasmFromModule(module) {
	const instance = new WebAssembly.Instance(module);
	const api = instance.exports;
	if (
		typeof api.haya32x64 !== "function" ||
		!(api.memory instanceof WebAssembly.Memory) ||
		typeof api.__heap_base?.value !== "number"
	) {
		throw new TypeError(
			"not a haya32x64 wasm module " +
			"(expected exports: haya32x64, memory, __heap_base)",
		);
	}
	const memory = api.memory;
	const output = api.__heap_base.value;
	const input = (output + 11) & ~3;
	let bytes = new Uint8Array(memory.buffer);
	let words = new DataView(memory.buffer);
	return {
		hash(data, seedLo, seedHi) {
			const needed = input + data.length;
			if (memory.buffer.byteLength < needed) {
				memory.grow(Math.ceil(
					(needed - memory.buffer.byteLength) / PAGE_SIZE,
				));
			}
			if (bytes.buffer !== memory.buffer) {
				bytes = new Uint8Array(memory.buffer);
				words = new DataView(memory.buffer);
			}
			bytes.set(data, input);
			api.haya32x64(output, input, data.length, seedLo, seedHi);
			return [
				words.getUint32(output, true),
				words.getUint32(output + 4, true),
			];
		},
	};
}

export function initWasm() {
	try {
		return initWasmFromModule(
			new WebAssembly.Module(decodeBase64(wasmBase64)),
		);
	} catch {
		return null;
	}
}
