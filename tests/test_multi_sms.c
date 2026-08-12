#include "test.h"
#include "multi_sms.h"
#include <string.h>
#include <stdlib.h>

// Multi-part SMS reassembly (modem/multi_sms.c).
// Parts can arrive out of order; ms_get() must join them correctly.

TEST(ms_single_part)
{
	ascii *text = NULL;

	ms_init();

	ASSERT_EQ(ms_incomming("hello", 5, 1, 1, 1), MS_SMS_DONE);
	ASSERT_EQ(ms_get(&text, 1), 5);
	ASSERT_STREQ(text, "hello");

	ms_delete(1);
	free(text);
}

TEST(ms_two_parts_in_order)
{
	ascii *text = NULL;

	ms_init();

	ASSERT_EQ(ms_incomming("part1:", 6, 7, 1, 2), MS_SMS_WAIT);
	ASSERT_EQ(ms_incomming("part2", 5, 7, 2, 2), MS_SMS_DONE);
	ASSERT_EQ(ms_get(&text, 7), 11);
	ASSERT_STREQ(text, "part1:part2");

	ms_delete(7);
	free(text);
}

TEST(ms_two_parts_out_of_order)
{
	ascii *text = NULL;

	ms_init();

	// second part arrives first
	ASSERT_EQ(ms_incomming("part2", 5, 3, 2, 2), MS_SMS_WAIT);
	ASSERT_EQ(ms_incomming("part1:", 6, 3, 1, 2), MS_SMS_DONE);

	// joined in correct order despite arrival order
	ASSERT_EQ(ms_get(&text, 3), 11);
	ASSERT_STREQ(text, "part1:part2");

	ms_delete(3);
	free(text);
}

TEST(ms_two_messages_interleaved)
{
	ascii *text = NULL;

	ms_init();

	// parts of two different messages interleave
	ASSERT_EQ(ms_incomming("A1", 2, 100, 1, 2), MS_SMS_WAIT);
	ASSERT_EQ(ms_incomming("B1", 2, 200, 1, 2), MS_SMS_WAIT);
	ASSERT_EQ(ms_incomming("A2", 2, 100, 2, 2), MS_SMS_DONE);
	ASSERT_EQ(ms_incomming("B2", 2, 200, 2, 2), MS_SMS_DONE);

	ASSERT_EQ(ms_get(&text, 100), 4);
	ASSERT_STREQ(text, "A1A2");
	free(text);
	text = NULL;

	ASSERT_EQ(ms_get(&text, 200), 4);
	ASSERT_STREQ(text, "B1B2");
	free(text);

	ms_delete(100);
	ms_delete(200);
}

TEST(ms_delete_removes_all_parts)
{
	ascii *text = NULL;

	ms_init();

	ms_incomming("part1", 5, 5, 1, 2);
	ms_incomming("part2", 5, 5, 2, 2);

	ms_delete(5);

	// after delete the message is gone
	ASSERT_EQ(ms_get(&text, 5), 0);
}

TEST(ms_part_too_long)
{
	ms_init();

	// parts longer than 160 are rejected
	ASSERT_EQ(ms_incomming("x", 161, 9, 1, 1), MS_SMS_ERROR);
}

TEST(ms_buffer_exhaustion)
{
	int i;

	ms_init();

	// MSMS_BUF_LEN == 10 — first 10 ids fill the buffer,
	// the 11th evicts old buffers
	for (i = 0; i < 10; i++)
	{
		ASSERT_EQ(ms_incomming("p1", 2, (u8)(i + 1), 1, 2), MS_SMS_WAIT);
	}

	// no error — old buffers are cleared
	ASSERT_EQ(ms_incomming("p1", 2, 11, 1, 2), MS_SMS_WAIT);
}
