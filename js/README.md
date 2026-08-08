# haya32x64 for JavaScript

`haya32x64` is a non-cryptographic 64-bit hash whose state and arithmetic are
strictly 32-bit. The package uses `Math.imul` plus exact limb arithmetic in
pure JavaScript and the reference C header compiled to WebAssembly for larger
inputs. It never requires `BigInt`.

```js
import { haya32x64, haya32x64Hex } from "haya32x64";

const words = haya32x64("hello world"); // [low32, high32]
console.log(haya32x64Hex("hello world"));
```

The seed is supplied the same way as the digest: low 32-bit word first.

```js
haya32x64(bytes, 0xcafebabe, 0xdeadbeef);
```

Use `haya32x64Pure` to force the no-wasm implementation. In runtimes that
precompile `.wasm` imports, pass the shipped module to `setWasmModule`.
