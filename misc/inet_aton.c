#include "system.h"

#ifdef HAVE_NETINET_IN_SYSTM_H
# include <netinet/in_systm.h>
#endif

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "inet_aton.h"

int
inet_aton(const char *cp, struct in_addr *inp)
{
    long addr;

    addr = inet_addr(cp);
    if (addr == ((long) -1)) return 0;

    memcpy(inp, &addr, sizeof(addr));
    return 1;
}
