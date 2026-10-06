#include "test.h"
#include "net_target.h"

// net_target_parse() classifies a "<host>:<port>" configuration string as a
// literal IP, a name to resolve, or invalid.  A numeric-but-invalid address
// must never be handed to the resolver.

static char _host[NET_TARGET_HOST_MAX];
static u16  _port;
static u32  _ip;

static net_target_type_e _parse(const char *url)
{
    _host[0] = '\0';
    _port = 0;
    _ip = 0;
    return (net_target_parse(url, _host, sizeof(_host), &_port, &_ip));
}

TEST(target_literal_ip)
{
    ASSERT_EQ(_parse("177.78.98.199:8080"), NET_TARGET_IP);
    ASSERT_EQ(_ip, 177u | (78u<<8) | (98u<<16) | (199u<<24));
    ASSERT_EQ(_port, 8080);
    ASSERT_STREQ(_host, "177.78.98.199");
}

TEST(target_name)
{
    ASSERT_EQ(_parse("www.gmmultimedia.cz:5013"), NET_TARGET_NAME);
    ASSERT_EQ(_port, 5013);
    ASSERT_EQ(_ip, 0);
    ASSERT_STREQ(_host, "www.gmmultimedia.cz");
}

TEST(target_name_with_digits)
{
    ASSERT_EQ(_parse("tracker1.example.com:80"), NET_TARGET_NAME);
    ASSERT_EQ(_port, 80);
}

TEST(target_default_unconfigured)
{
    // the shipped default — must be invalid, not looked up
    ASSERT_EQ(_parse("0.0.0.0:4444"), NET_TARGET_INVALID);
    ASSERT_EQ(_port, 0);
    ASSERT_EQ(_ip, 0);
}

TEST(target_missing_port)
{
    ASSERT_EQ(_parse("example.com"), NET_TARGET_INVALID);
    ASSERT_EQ(_parse("1.2.3.4"), NET_TARGET_INVALID);
    ASSERT_EQ(_port, 0);
}

TEST(target_bad_port)
{
    ASSERT_EQ(_parse("example.com:0"), NET_TARGET_INVALID);
    ASSERT_EQ(_parse("example.com:65536"), NET_TARGET_INVALID);
    ASSERT_EQ(_parse("example.com:abc"), NET_TARGET_INVALID);
    ASSERT_EQ(_port, 0);
}

TEST(target_bad_ip)
{
    ASSERT_EQ(_parse("256.1.2.3:80"), NET_TARGET_INVALID);
    ASSERT_EQ(_parse("1.2.3.0:80"), NET_TARGET_INVALID);
    ASSERT_EQ(_parse("1.2.3:80"), NET_TARGET_INVALID);
    ASSERT_EQ(_parse("1234:80"), NET_TARGET_INVALID);   // no letters, not an IP
    ASSERT_EQ(_port, 0);
}

TEST(target_host_too_long)
{
    ASSERT_EQ(_parse("averyveryverylongdomainnamethatdoesnotfitinthebuffer.example.com:80"),
              NET_TARGET_INVALID);
    ASSERT_EQ(_port, 0);
}
