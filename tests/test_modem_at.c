#include "test.h"
#include "modem.h"
#include "modem_at.h"

// ---- modem_at_param_pos ----
//
// Returns pointer after the n-th comma, or the original string if the
// string ends before n commas are found.

TEST(param_pos_first_param)
{
	// +CMTI: "SM",2  →  n=1 skips one comma → "2"
	const char *s = "\"SM\",2";
	const char *p = modem_at_param_pos(s, 1);

	ASSERT_STREQ(p, "2");
}

TEST(param_pos_middle_param)
{
	// a,b,c  →  n=2 skips two commas → "c"
	const char *s = "a,b,c";
	const char *p = modem_at_param_pos(s, 2);

	ASSERT_STREQ(p, "c");
}

TEST(param_pos_zero)
{
	// n=0 → original pointer (no comma skipped)
	const char *s = "abc,def";
	const char *p = modem_at_param_pos(s, 0);

	ASSERT(p == s);
}

TEST(param_pos_not_enough_commas)
{
	// n=5 but only 1 comma → fallback to original pointer
	const char *s = "a,b";
	const char *p = modem_at_param_pos(s, 5);

	ASSERT(p == s);
}

TEST(param_pos_empty_string)
{
	const char *s = "";
	const char *p = modem_at_param_pos(s, 1);

	ASSERT(p == s);
}

// ---- modem_parse_pattern ----
//
// Returns NULL if the string does not start with the pattern,
// otherwise pointer after the pattern with leading spaces skipped.

TEST(parse_pattern_basic)
{
	char *p = modem_parse_pattern("+CREG: 0,1", "+CREG:");

	ASSERT(p != NULL);
	ASSERT_STREQ(p, "0,1");
}

TEST(parse_pattern_quoted)
{
	char *p = modem_parse_pattern("+CMTI: \"SM\",2", "+CMTI:");

	ASSERT(p != NULL);
	ASSERT_STREQ(p, "\"SM\",2");
}

TEST(parse_pattern_space_skip)
{
	// spaces after the pattern are skipped
	char *p = modem_parse_pattern("+CLIP:  \"123456\"", "+CLIP:");

	ASSERT(p != NULL);
	ASSERT_STREQ(p, "\"123456\"");
}

TEST(parse_pattern_no_match)
{
	ASSERT(modem_parse_pattern("OK", "+CREG:") == NULL);
}

TEST(parse_pattern_leading_space_not_allowed)
{
	// the pattern must match at the very start — no leading-space skip
	ASSERT(modem_parse_pattern(" +CREG: 1", "+CREG:") == NULL);
}

TEST(parse_pattern_exact_match)
{
	// full-string match → pointer to the terminating '\0'
	char *p = modem_parse_pattern("RING", "RING");

	ASSERT(p != NULL);
	ASSERT_EQ(p[0], '\0');
}

TEST(parse_pattern_prefix_semantics)
{
	// strncmp-based prefix match
	char *p = modem_parse_pattern("OK", "O");

	ASSERT(p != NULL);
	ASSERT_STREQ(p, "K");
}
