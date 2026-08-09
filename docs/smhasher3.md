# Running SMHasher3

The harness pins SMHasher3 to commit
`51d3cd1ac0aa4934f6aacb44d9d234f50300b6e3`. The framework is fetched at test
time and is not part of release artifacts.

```sh
make -C tests/smhasher3 run
```

The dedicated adapter is stored at `tests/smhasher3/haya32x64.cpp`. To print
both computed verification values:

```sh
make -C tests/smhasher3 verify
```

Expected values:

| mode | verification |
|---|---:|
| canonical little endian | `0x431563D2` |
| byte-swapped | `0x65BBCA3D` |

Digests for inputs below 128 bytes are unchanged across every revision back
to the original `0xEAA8E435` digest; the values above reflect the pair-lane
bulk kernel used for inputs of 128 bytes and above.

## Speed comparisons

SMHasher3 is also used as the common native harness for small-key and bulk
speed comparisons:

```sh
make benchmark-native > native.txt
```

That target builds the pinned checkout with `-O3 -march=native -DNDEBUG` and
runs the comparator set on one CPU. The extra `-include cstdlib` flag works
around a missing include in the pinned upstream tree when building with GCC
15 on AArch64. It does not alter the hash implementations. See the
[benchmark methodology and results](benchmarks.md) for the comparator
rationale, JavaScript harness, and checked-in raw output.

A SMHasher3 speed run is not a quality run and does not imply that every
listed comparator passes the statistical test suite.

SMHasher3 is GPL-3.0-or-later. Fetching it separately keeps its test framework
out of the public-domain library and npm artifacts; redistribution of a linked
test executable must follow the GPL.
