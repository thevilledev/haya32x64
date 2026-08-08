#!/bin/sh
set -eu
cd "$(dirname "$0")/.."

if [ -n "${WASM_CC:-}" ]; then
	set -- "$WASM_CC" --target=wasm32-unknown-unknown
elif command -v zig >/dev/null 2>&1; then
	set -- zig cc --target=wasm32-freestanding
else
	set -- clang --target=wasm32-unknown-unknown
fi

"$@" -O3 -nostdlib \
	-fno-sanitize=undefined \
	-Wall -Wextra -Werror \
	-isystem wasm/include \
	-Wl,--no-entry \
	-Wl,--export-memory \
	-Wl,--export=__heap_base \
	-Wl,-z,stack-size=32768 \
	-Wl,--strip-all \
	-o wasm/haya32x64.wasm wasm/shim.c

# Linkers create executable outputs even though a wasm module is package data.
chmod 0644 wasm/haya32x64.wasm

node scripts/embed.mjs
wc -c wasm/haya32x64.wasm
