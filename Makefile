CORPUS ?= $(CURDIR)/tests/differential/corpus.bin
DIFFERENTIAL_SEED ?= 0x0123456789abcdef
DIFFERENTIAL_CASES ?= 4096

.PHONY: all test check c-test js-test differential wasm smhasher3 \
	benchmark-js benchmark-native clean

all: test

test: c-test differential

check: test
	npm run test:package --prefix js

c-test:
	$(MAKE) -C tests test

js-test:
	npm test --prefix js

tests/differential/generate: tests/differential/generate.c haya32x64.h
	$(MAKE) -C tests generate

differential: tests/differential/generate
	tests/differential/generate \
		"$(CORPUS)" "$(DIFFERENTIAL_SEED)" "$(DIFFERENTIAL_CASES)"
	HAYA32X64_CORPUS="$(CORPUS)" npm test --prefix js

wasm:
	npm run build:wasm --prefix js

smhasher3:
	$(MAKE) -C tests/smhasher3 run

benchmark-js:
	npm ci --prefix bench
	npm run bench --prefix bench

benchmark-native:
	$(MAKE) -C tests/smhasher3 \
		EXTRA_CXXFLAGS='-O3 -march=native -DNDEBUG -include cstdlib' build
	./bench/native.sh

clean:
	$(MAKE) -C tests clean
	rm -f "$(CORPUS)"
