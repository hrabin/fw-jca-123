#ifndef NET_TARGET_H
#define NET_TARGET_H

#include "type.h"

#define NET_TARGET_HOST_MAX 64

typedef enum {
    NET_TARGET_INVALID = 0,
    NET_TARGET_IP,     // host is a literal dotted-quad, *ip is set
    NET_TARGET_NAME,   // host has to be resolved, *ip stays 0
} net_target_type_e;

// Parse a "<host>:<port>" target string from the configuration.
//   literal IPv4 address  -> NET_TARGET_IP     (*ip and *port set)
//   name with a letter    -> NET_TARGET_NAME   (*port set, *ip = 0)
//   anything else         -> NET_TARGET_INVALID (*port untouched)
// A numeric-but-invalid address (e.g. "0.0.0.0:4444") is INVALID, never a name,
// so an unconfigured target is not sent to the resolver.
net_target_type_e net_target_parse (const ascii *url, char *host, size_t host_size,
                                    u16 *port, u32 *ip);

#endif // ! NET_TARGET_H
