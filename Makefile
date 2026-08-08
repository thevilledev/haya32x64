.PHONY: all test c-test js-test wasm smhasher3 clean

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

clean:
	$(MAKE) -C tests clean
