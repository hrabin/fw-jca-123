# Unit Tests — fw-jca-123

Host-side (x86) unit tests in plain C. No framework dependencies.

## Quick start

```bash
make test
```

This compiles the test runner with all `test_*.c` files and the modules under
test, then runs the suite.

## Adding a new test

```bash
make add                    # prompts for a name, creates skeleton
# or manually:
cp test_nmea.c test_mymodule.c
```

Edit the new file — each `TEST(name) { ... }` block auto-registers:

```c
#include "test.h"

TEST(my_first_test)
{
    ASSERT(1 + 1 == 2);
    ASSERT_EQ(42, answer);
}
```

No registration boilerplate — just write `TEST()` blocks.

## Available assertions

| Macro | Description |
|-------|-------------|
| `ASSERT(cond)` | Condition is true |
| `ASSERT_EQ(a, b)` | Two integers are equal |
| `ASSERT_STREQ(a, b)` | Two strings are equal |

Each assertion failure prints file:line and stops the current test (remaining
assertions in that test are skipped).

## How it works

- `test.h` — framework (macros + assertions + runner declaration)
- `test_main.c` — `main()` and the test runner loop
- `test_*.c` — test files, auto-discovered via `$(wildcard test_*.c)`

Tests register at static-init time via a linked list (`test_list_head`).
`test_run_all()` iterates the list, runs each test, and reports pass/fail.

## Stub layer

The `stubs/` directory provides minimal header replacements so embedded code
can compile on the host:

| Stub | Replaces | Purpose |
|------|----------|---------|
| `common.h` | SDK common.h | Wraps standard C headers |
| `os.h` | SDK os.h | No-op RTOS primitives (timers, semaphores, delays) |
| `platform_setup.h` | SDK platform_setup.h | `PACK`, `NL`, `ABS` macros, includes `util.h` |
| `log.h` | SDK log.h | No-op logging macros |
| `cfg.h` | app/cfg.h | `cfg_read()` returns false, `cfg_read_static()` returns NULL |
| `const.h` | app/const.h | `MAX_PHONENUM_LEN`, `MAX_SMS_LEN` |

Real SDK headers (`type.h`, `rtc.h`, `rtc_type.h`, `buf.h`, `parse.h`, `util.h`)
compile as-is on the host and are used directly from `../sdk/`.

## Modules under test

| Source | Tested by |
|--------|-----------|
| `lib/nmea/nmea.c` | `test_nmea.c` — GGA/RMC/GSV parsing, checksum |
| `modem/modem.c`, `modem/modem_at.c` | `test_modem_at.c` — AT parameter extraction, URC pattern matching |
| `sdk/common/parse.c`, `buf.c`, `util.c` | compiled as support code |

## Directory layout

```
tests/
├── README.md
├── Makefile
├── test.h              # framework
├── test_main.c         # runner main()
├── test_nmea.c         # NMEA parser tests
├── test_modem_at.c     # modem AT/URC parsing tests
├── test_*.c            # additional test files (auto-discovered)
└── stubs/              # host-compilation stubs
    ├── common.h
    ├── cfg.h
    ├── const.h
    ├── log.h
    ├── os.h
    └── platform_setup.h
```
