# Reproducible SMHasher3 harness for the validated haya32x64 candidate.
# SMHasher3 is GPL-3.0-or-later and fetched only for testing; the algorithm
# source copied into its tree remains public domain.

SMHASHER3_URL    = https://gitlab.com/fwojcik/smhasher3.git
SMHASHER3_COMMIT = 51d3cd1ac0aa4934f6aacb44d9d234f50300b6e3

SRC    = smhasher3
BUILD ?= build
HASH  ?= haya32x64
EXTRA_CXXFLAGS ?=

.PHONY: all install build verify run clean distclean

all: build

$(SRC)/.git:
	rm -rf $(SRC).tmp
	git clone $(SMHASHER3_URL) $(SRC).tmp
	git -C $(SRC).tmp checkout --detach $(SMHASHER3_COMMIT)
	mv $(SRC).tmp $(SRC)

# exp/hayaexp.cpp is the exact adapter used for the validated three-candidate
# run. It registers haya32x64 alongside the other two experiments; HASH above
# selects only the hash this repository ships.
install: $(SRC)/.git ../../exp/hayaexp.cpp
	cp ../../exp/hayaexp.cpp $(SRC)/hashes/hayaexp.cpp
	@if grep -q 'hashes/hayaexp.cpp' $(SRC)/hashes/Hashsrc.cmake; then \
		echo "Hashsrc.cmake already lists hayaexp.cpp"; \
	else \
		awk '/^\)$$/ && !done { print "  hashes/hayaexp.cpp"; done = 1 } { print }' \
			$(SRC)/hashes/Hashsrc.cmake > $(SRC)/hashes/Hashsrc.cmake.new; \
		mv $(SRC)/hashes/Hashsrc.cmake.new $(SRC)/hashes/Hashsrc.cmake; \
	fi

build: install
	mkdir -p $(SRC)/$(BUILD)
	cd $(SRC)/$(BUILD) && CXXFLAGS="$(EXTRA_CXXFLAGS)" cmake ..
	$(MAKE) -C $(SRC)/$(BUILD) -j$$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)

verify: build
	cd $(SRC)/$(BUILD) && \
		./SMHasher3 --test=VerifyAll --verbose 2>&1 | grep -i haya32x64

run: build
	cd $(SRC)/$(BUILD) && ./SMHasher3 --version
	cd $(SRC)/$(BUILD) && ./SMHasher3 $(HASH)

clean:
	rm -rf $(SRC)/$(BUILD)

distclean:
	rm -rf $(SRC) $(SRC).tmp
