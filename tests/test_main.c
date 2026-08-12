#include "test.h"
#include <stdlib.h>

test_entry_t *test_list_head = NULL;
int _asserts_passed = 0;
int _asserts_failed = 0;

void test_register(const char *name, void (*func))
{
	test_entry_t *e = malloc(sizeof(test_entry_t));
	e->name = name;
	e->func = func;
	e->next = test_list_head;
	test_list_head = e;
}

int test_run_all(void)
{
	int ran = 0, passed = 0;

	printf("\n");
	for (test_entry_t *e = test_list_head; e; e = e->next)
	{
		ran++;
		_asserts_passed = 0;
		_asserts_failed = 0;

		e->func();

		if (_asserts_failed == 0)
		{
			passed++;
			printf("  PASS %-40s (%d asserts)\n", e->name, _asserts_passed);
		}
		else
		{
			printf("  FAIL %-40s (%d/%d asserts failed)\n",
			       e->name, _asserts_failed, _asserts_passed);
		}
	}
	printf("\n  %d/%d tests passed\n\n", passed, ran);
	return (passed == ran) ? 0 : 1;
}

int main(void)
{
	return test_run_all();
}
