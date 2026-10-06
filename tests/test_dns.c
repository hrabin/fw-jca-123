#include "test.h"
#include "modem_bg95.h"

// modem_bg95_urc() parses the BG95 DNS answer, which arrives after OK:
//   +QIURC: "dnsgip",<err>,<count>,<ttl>    header (err != 0 on failure)
//   +QIURC: "dnsgip","<ip>"                 one URC per resolved address
// Some firmware merges the first address into the header line; both are tested.

static bool _done;
static bool _ok;
static u32  _ip;

static void _reset(void)
{
    _done = false;
    _ok = false;
    _ip = 0;
}

TEST(dnsgip_header_then_ip)
{
    _reset();

    ASSERT(modem_bg95_dnsgip_parse("+QIURC: \"dnsgip\",0,2,399", &_done, &_ok, &_ip));
    ASSERT(!_done);              // the address(es) are still to come
    ASSERT(!_ok);
    ASSERT_EQ(_ip, 0);

    ASSERT(modem_bg95_dnsgip_parse("+QIURC: \"dnsgip\",\"220.181.57.216\"", &_done, &_ok, &_ip));
    ASSERT(_done);
    ASSERT(_ok);
    ASSERT_EQ(_ip, 220u | (181u<<8) | (57u<<16) | (216u<<24));
}

TEST(dnsgip_merged_single_line)
{
    _reset();

    ASSERT(modem_bg95_dnsgip_parse("+QIURC: \"dnsgip\",0,1,600,\"1.2.3.4\"", &_done, &_ok, &_ip));
    ASSERT(_done);
    ASSERT(_ok);
    ASSERT_EQ(_ip, 1u | (2u<<8) | (3u<<16) | (4u<<24));
}

TEST(dnsgip_error)
{
    _reset();
    _ok = true;                  // must be cleared by the error line

    ASSERT(modem_bg95_dnsgip_parse("+QIURC: \"dnsgip\",565", &_done, &_ok, &_ip));
    ASSERT(_done);
    ASSERT(!_ok);
}

TEST(dnsgip_unrelated_urc)
{
    _reset();

    ASSERT(!modem_bg95_dnsgip_parse("+QIURC: \"recv\",0", &_done, &_ok, &_ip));
    ASSERT(!modem_bg95_dnsgip_parse("OK", &_done, &_ok, &_ip));
    ASSERT(!modem_bg95_dnsgip_parse("+QIOPEN: 0,0", &_done, &_ok, &_ip));
    ASSERT(!_done);
}

TEST(dnsgip_bad_address_ignored)
{
    _reset();

    ASSERT(modem_bg95_dnsgip_parse("+QIURC: \"dnsgip\",0,1,600,\"300.1.2.3\"", &_done, &_ok, &_ip));
    ASSERT(!_done);              // out-of-range octet is not a valid answer
    ASSERT(!_ok);
}
