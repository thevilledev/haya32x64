# Running SMHasher3

The harness pins SMHasher3 to commit
`51d3cd1ac0aa4934f6aacb44d9d234f50300b6e3`. The framework is fetched at test
time and is not part of release artifacts.

```sh
make -C tests/smhasher3 run
```

The current adapter is the validated multi-candidate registration from
`exp/hayaexp.cpp`; the harness selects only `haya32x64`. To print both computed
verification values:

```sh
make -C tests/smhasher3 verify
```

Expected values:

| mode | verification |
|---|---:|
| canonical little endian | `0x7137E6DC` |
| byte-swapped | `0x1DFB2C55` |

SMHasher3 is GPL-3.0-or-later. Fetching it separately keeps its test framework
out of the public-domain library and npm artifacts; redistribution of a linked
test executable must follow the GPL.
