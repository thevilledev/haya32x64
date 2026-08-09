.PHONY: all test c-test js-test wasm smhasher3 benchmark-js benchmark-native clean

all: test

test: c-test js-test

c-test:
	$(MAKE) -C tests test

js-test:
	npm test --prefix js

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
