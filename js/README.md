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

Unknown-length streaming produces the same digest as concatenating the input
and hashing it in one shot. `digest()` is non-destructive, so the stream can be
extended afterward.

```js
import { createHaya32x64 } from "haya32x64";

const stream = createHaya32x64(0xcafebabe, 0xdeadbeef);
stream.update(part1).update(part2);
const words = stream.digest();
```

The streaming engine is pure JavaScript and retains at most 192 input bytes.
Each string passed to `update()` is UTF-8 encoded independently; use byte
chunks when splitting arbitrary string positions (such as a surrogate pair).
