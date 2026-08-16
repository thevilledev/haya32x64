# Differential conformance

`make test` and `make differential` from the repository root generate a
C-reference corpus and replay it through both JavaScript engines. The
manual equivalent is:

```sh
cc -O2 -std=c11 -Wall -Wextra -Werror generate.c -o generate
./generate /tmp/haya32x64.bin 0x0123456789abcdef 4096
HAYA32X64_CORPUS=/tmp/haya32x64.bin node --test ../../js/test/differential.test.mjs
```

Lengths 0 through 384 are exhaustive. Remaining cases emphasize the 128-byte
bulk boundary and powers of two through 128 KiB. The logged seed reproduces a
corpus exactly.
