#!/bin/sh
set -eu

binary=${1:-tests/smhasher3/smhasher3/build/SMHasher3}
cpu=${HAYA_BENCH_CPU:-2}

run_pinned() {
	if command -v taskset >/dev/null 2>&1; then
		taskset -c "$cpu" "$@"
	else
		"$@"
	fi
}

printf '%s\n' '=== benchmark metadata ==='
date -u '+utc=%Y-%m-%dT%H:%M:%SZ'
uname -a
cc --version | sed -n '1p'
"$binary" --version
printf 'revision='
git rev-parse HEAD
printf 'pinned_cpu=%s\n' "$cpu"

for hash in \
	haya32x64 \
	khashv-64 \
	lookup3 \
	MurmurHash2-64.int32 \
	t1ha0 \
	a5hash-32 \
	XXH-32 \
	a5hash-128.64 \
	XXH3-64 \
	rapidhash \
	wyhash.strict \
	ChibiHash2 \
	MuseAir.bfast
do
	printf '\n=== %s ===\n' "$hash"
	run_pinned "$binary" --test=SpeedSmall,SpeedBulk --ncpu=1 "$hash"
done
