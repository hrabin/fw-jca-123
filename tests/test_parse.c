#include "test.h"
#include "parse.h"

// Command-parser primitives — parse.c is the foundation for all
// "CFG=12,\"value\"" style command parsing.

// ---- parse_number ----

TEST(parse_number_positive)
{
	parse_number_t n = 0;
	const char *s = "42";
	const char *p = parse_number(&n, s);

	ASSERT(p != NULL);
	ASSERT_EQ(n, 42);
	ASSERT_EQ(*p, '\0');
}

TEST(parse_number_negative)
{
	parse_number_t n = 0;
	const char *p = parse_number(&n, "-7");

	ASSERT(p != NULL);
	ASSERT_EQ(n, -7);
}

TEST(parse_number_trailing_text)
{
	parse_number_t n = 0;
	const char *p = parse_number(&n, "42abc");

	ASSERT(p != NULL);
	ASSERT_EQ(n, 42);
	ASSERT_STREQ(p, "abc");
}

TEST(parse_number_not_a_number)
{
	parse_number_t n = 0;
	const char *p = parse_number(&n, "abc");

	ASSERT(p == NULL);
}

// ---- parse_u64 ----

TEST(parse_u64_basic)
{
	u64 v = 0;
	const char *p = parse_u64(&v, "123456789012");

	ASSERT(p != NULL);
	ASSERT_EQ(v, 123456789012ULL);
	ASSERT_EQ(*p, '\0');
}

// ---- parse_string ----

TEST(parse_string_basic)
{
	char dest[16];
	const char *s = "\"internet\"";
	const char *p = parse_string(dest, s, sizeof(dest));

	ASSERT(p != NULL);
	ASSERT_STREQ(dest, "internet");
	ASSERT_EQ(*p, '\0');
}

TEST(parse_string_leading_spaces)
{
	char dest[16];
	const char *p = parse_string(dest, "  \"abc\"", sizeof(dest));

	ASSERT(p != NULL);
	ASSERT_STREQ(dest, "abc");
}

TEST(parse_string_trailing_text)
{
	char dest[16];
	const char *p = parse_string(dest, "\"abc\",rest", sizeof(dest));

	ASSERT(p != NULL);
	ASSERT_STREQ(dest, "abc");
	ASSERT_STREQ(p, ",rest");
}

TEST(parse_string_no_quote)
{
	char dest[16];
	ASSERT(parse_string(dest, "abc", sizeof(dest)) == NULL);
}

TEST(parse_string_unterminated)
{
	char dest[16];
	ASSERT(parse_string(dest, "\"abc", sizeof(dest)) == NULL);
}

TEST(parse_string_too_long)
{
	char dest[4];
	ASSERT(parse_string(dest, "\"abcdef\"", sizeof(dest)) == NULL);
}

// ---- parse_separator ----

TEST(parse_separator_basic)
{
	const char *p = parse_separator(",value");

	ASSERT(p != NULL);
	ASSERT_STREQ(p, "value");
}

TEST(parse_separator_spaces)
{
	const char *p = parse_separator("  ,value");

	ASSERT(p != NULL);
	ASSERT_STREQ(p, "value");
}

TEST(parse_separator_no_comma)
{
	ASSERT(parse_separator("value") == NULL);
}

// ---- parse_terminator ----

TEST(parse_terminator_end)
{
	const char *p = parse_terminator("");

	ASSERT(p != NULL);
	ASSERT_EQ(*p, '\0');
}

TEST(parse_terminator_not_end)
{
	ASSERT(parse_terminator("x") == NULL);
}
