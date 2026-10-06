// Shared host mock for modem symbols pulled in by modem_bg95.c.
// The module's DNS parsing is tested directly (test_dns.c); the application
// notification it emits is not needed on the host.
#include "type.h"

void modem_main_jamming(bool state)
{
    (void)state;
}
