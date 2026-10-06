#include "common.h"
#include "net_target.h"

// _is_ip_chars: true for a string made only of digits and dots (candidate IPv4)
static bool _is_ip_chars (const char *s)
{
    for (; *s != '\0'; s++)
    {
        if (!isdigit((unsigned char)*s) && (*s != '.'))
            return (false);
    }
    return (true);
}

static bool _has_alpha (const char *s)
{
    for (; *s != '\0'; s++)
    {
        if (isalpha((unsigned char)*s))
            return (true);
    }
    return (false);
}

static bool _ipv4_parse (const char *s, u32 *ip)
{
    s32 a,b,c,d;

    if (sscanf(s, "%" SCNd32 ".%" SCNd32 ".%" SCNd32 ".%" SCNd32,
               &a, &b, &c, &d) != 4)
        return (false);

    if ((a>255) || (b>255) || (c>255) || (d>255))
        return (false);
    if ((a==0) || (d==0)) // 0.x.x.x / x.x.x.0 are not usable host addresses
        return (false);

    *ip = a + (b<<8) + (c<<16) + (d<<24);
    return (true);
}

net_target_type_e net_target_parse (const ascii *url, char *host, size_t host_size,
                                    u16 *port, u32 *ip)
{
    const char *colon;
    size_t len;
    s32 p;

    if ((url == NULL) || (host == NULL) || (port == NULL) || (ip == NULL))
        return (NET_TARGET_INVALID);

    *ip = 0;

    // expected format "host.name:8080" or "177.78.98.199:8080"
    colon = strrchr(url, ':');
    if (colon == NULL)
        return (NET_TARGET_INVALID);

    len = colon - url;
    if ((len == 0) || (len >= host_size))
        return (NET_TARGET_INVALID);

    if ((sscanf(colon + 1, "%" SCNd32, &p) != 1) || (p <= 0) || (p > 65535))
        return (NET_TARGET_INVALID);

    memcpy(host, url, len);
    host[len] = '\0';

    if (_is_ip_chars(host))
    {   // must be a valid IPv4 address, never a DNS name
        if (_ipv4_parse(host, ip))
        {
            *port = (u16)p;
            return (NET_TARGET_IP);
        }
        return (NET_TARGET_INVALID);
    }

    if (_has_alpha(host))
    {
        *port = (u16)p;
        return (NET_TARGET_NAME);
    }

    return (NET_TARGET_INVALID);
}
