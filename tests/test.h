// Minimal C unit-test framework — no external dependencies.
//
// Usage in a test file:
//   #include "test.h"
//
//   TEST(test_name) {
//       ASSERT(1 + 1 == 2);
//       ASSERT_EQ(42, answer);
//   }
//
// Each TEST() block auto-registers into a global linked list.
// test_run_all() discovers and executes all tests.

#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <string.h>

// ---- test registry ----

typedef struct test_entry {
	const char *name;
	void (*func)(void);
	struct test_entry *next;
} test_entry_t;

extern test_entry_t *test_list_head;
void test_register(const char *name, void (*func));

#define TEST(name)                                                       \
	static void test_##name(void);                                       \
	__attribute__((constructor))                                         \
	static void _reg_##name(void) { test_register(#name, test_##name); } \
	static void test_##name(void)

// ---- internal counters (reset per-test by runner) ----

extern int _asserts_passed;
extern int _asserts_failed;

// ---- assertions ----

#define ASSERT(cond) do {                                                \
	_asserts_passed++;                                                   \
	if (!(cond)) {                                                       \
		_asserts_failed++;                                               \
		fprintf(stderr, "  FAIL %s:%d: ASSERT(%s)\n",                    \
		        __FILE__, __LINE__, #cond);                              \
		return;                                                          \
	}                                                                    \
} while (0)

#define ASSERT_EQ(a, b) do {                                             \
	_asserts_passed++;                                                   \
	long long _va = (long long)(a);                                      \
	long long _vb = (long long)(b);                                      \
	if (_va != _vb) {                                                    \
		_asserts_failed++;                                               \
		fprintf(stderr, "  FAIL %s:%d: ASSERT_EQ(%s, %s)  "             \
		        "%lld != %lld\n",                                        \
		        __FILE__, __LINE__, #a, #b, _va, _vb);                  \
		return;                                                          \
	}                                                                    \
} while (0)

#define ASSERT_STREQ(a, b) do {                                          \
	_asserts_passed++;                                                   \
	const char *_va = (a);                                               \
	const char *_vb = (b);                                               \
	if (strcmp(_va, _vb) != 0) {                                         \
		_asserts_failed++;                                               \
		fprintf(stderr, "  FAIL %s:%d: ASSERT_STREQ(\"%s\", \"%s\")\n", \
		        __FILE__, __LINE__, _va, _vb);                          \
		return;                                                          \
	}                                                                    \
} while (0)

// ---- runner ----

int test_run_all(void);

#endif // TEST_H
