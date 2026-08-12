#include "test.h"
#include "text_lib.h"
#include "const.h"
#include <string.h>

// Text utilities from sdk/common/text_lib.c — phone validation,
// ASCII detection, phone comparison, text truncation.

// ---- text_is_valid_phone ----

TEST(phone_valid_plain)
{
    ASSERT(text_is_valid_phone("123"));
    ASSERT(text_is_valid_phone("12345678901234567890"));  // 20 chars max
}

TEST(phone_valid_plus_prefix)
{
    ASSERT(text_is_valid_phone("+420123456789"));
}

TEST(phone_too_short)
{
    ASSERT(!text_is_valid_phone("12"));
    ASSERT(!text_is_valid_phone(""));
}

TEST(phone_too_long)
{
    ASSERT(!text_is_valid_phone("123456789012345678901"));  // 21 chars
}

TEST(phone_invalid_chars)
{
    ASSERT(!text_is_valid_phone("12a45"));
    ASSERT(!text_is_valid_phone("12 45"));
}

TEST(phone_plus_only_first)
{
    // '+' is allowed only as the first character
    ASSERT(!text_is_valid_phone("4+20123"));
}

TEST(phone_null)
{
    ASSERT(!text_is_valid_phone(NULL));
}

// ---- text_is_ascii ----

TEST(ascii_plain)
{
    ascii s[] = "hello world";
    ASSERT(text_is_ascii(s));
}

TEST(ascii_utf8_detected)
{
    // 'ž' in UTF-8 has the high bit set
    ascii s[] = "\xC5\xBElu\xC5\xA5ou\xC4\x8Dk\xC3\xBD";  // "žluťoučký"
    ASSERT(!text_is_ascii(s));
}

TEST(ascii_empty)
{
    ascii s[] = "";
    ASSERT(text_is_ascii(s));
}

// ---- text_phone_compare ----

TEST(phone_compare_same)
{
    ASSERT(text_phone_compare("420123456789", "420123456789"));
}

TEST(phone_compare_prefix_difference)
{
    // compares from the end — +420 prefix does not matter
    ASSERT(text_phone_compare("420123456789", "+420123456789"));
}

TEST(phone_compare_different)
{
    ASSERT(!text_phone_compare("420111111111", "420222222222"));
}

TEST(phone_compare_empty)
{
    ASSERT(!text_phone_compare("", "123"));
    ASSERT(!text_phone_compare("123", ""));
}

// ---- text_limit ----

TEST(limit_no_truncation)
{
    ascii s[] = "short";
    ASSERT_STREQ(text_limit(s, 10), "short");
}

TEST(limit_truncates_with_dots)
{
    ascii s[] = "abcdefghij";
    ASSERT_STREQ(text_limit(s, 6), "abc...");
}

TEST(limit_short_limit_no_dots)
{
    ascii s[] = "abcdef";
    ASSERT_STREQ(text_limit(s, 2), "ab");
}
