# BSQ Test Harness

A comprehensive test harness for the `bsq` project. Generates and runs tests across three categories: valid maps, invalid maps, and large maps.

## Structure

```
.
├── main.c
├── Makefile
├── valid_maps/
│   └── Makefile
├── invalid_maps/
│   └── Makefile
└── large_maps/
    └── Makefile
```

The `bsq` binary is expected at `../bsq` (one level up from this directory).

## Build

```bash
make
```

## Usage

```bash
./test_harness [-s|--show-output] [valid|invalid|large|all]
```

| Argument | Description |
|---|---|
| `valid` | Run the 20 valid map tests |
| `invalid` | Run the 20 invalid map tests |
| `large` | Run the 2 large map tests |
| `all` | Run all categories (default) |
| `-s` / `--show-output` | Also print the raw `bsq` output for every test |

The `-s` flag can be combined with any mode:

```bash
./test_harness -s valid
./test_harness --show-output invalid
./test_harness -s
```

## Make Targets

From the root directory:

| Target | Command |
|---|---|
| Build | `make` |
| Run all tests | `make test` |
| Run all + show output | `make show` |
| Run valid only | `make valid` |
| Run invalid only | `make invalid` |
| Run large only | `make large` |
| Clean objects | `make clean` |
| Clean all | `make fclean` |
| Rebuild | `make re` |

From a subfolder, `make` runs that category's tests and `make show` runs them with output displayed:

```bash
cd valid_maps  && make       # runs valid tests
cd invalid_maps && make show  # runs invalid tests + shows bsq output
cd large_maps  && make       # runs large tests
```

## Test Categories

### Valid Maps (20 tests)

Random grids between 3×3 and 25×25 with randomly chosen empty, obstacle, and square characters. Each map is solved by a reference DP implementation and the result is compared byte-for-byte against `bsq` output.

Output on pass:
```
[PASS] valid_maps/map_001  (12x9, sq=4)
```

Output on fail:
```
[FAIL] valid_maps/map_002  (7x7, sq=2)
  exp: ......
  got: .xxxx.
```

### Invalid Maps (20 tests — 10 error types × 2)

Each type triggers a specific error path. All invalid maps must produce exactly `Map Error\n`.

| Type | Description | Error |
|---|---|---|
| 0 | No row count in header | `ERR_MAP_INFO` |
| 1 | Non-numeric prefix in header | `ERR_INV_NUM` |
| 2 | Zero row count | `ERR_MAP_INFO` |
| 3 | Too few rows (header says 5, file has 3) | `ERR_MAP_CHAR` |
| 4 | Invalid character `!` in map body | `ERR_MAP_CHAR` |
| 5 | Short row (one row has `cols-1` characters) | `ERR_MAP_CHAR` |
| 6 | Extra columns in a row (`cols+2` characters) | `ERR_MAP_COL` |
| 7 | Empty file | `ERR_MAP_INFO` |
| 8 | `INT_MAX` rows declared, only 1 body row | `ERR_MAP_CHAR` |
| 9 | Duplicate chars in header (`emp == obs`) | `ERR_MAP_INFO` |

### Large Maps (2 tests)

All-empty maps with random dimensions up to 30,000 × 50,000. Since no obstacles exist, the largest square fills `min(rows, cols)`. The test verifies only the first row of output (first char is the square char, char at index `max_sq` is the empty char).

A `Map Error` result is accepted as a pass — it indicates `bsq` hit a memory limit on the DP matrix, which is expected behaviour for very large inputs.

## Exit Code

The binary exits with the number of failed tests (0 = all passed).
