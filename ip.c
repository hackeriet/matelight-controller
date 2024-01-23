#include <stddef.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <net/if.h>

#include "game.h"

char ip_address[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];

void ip_init(void)
{
    struct ifaddrs *ifa, *ifptr;
    struct sockaddr_in *addr;
    struct sockaddr_in6 *addr6;

    strcpy(ip_address, "127.0.0.1");

    if (getifaddrs(&ifa) != 0)
        return;

    for (ifptr = ifa; ifptr; ifptr = ifptr->ifa_next) {
        if (! (ifptr->ifa_flags & IFF_UP))
            continue;
        if ((ifptr->ifa_flags & IFF_LOOPBACK))
            continue;
        if (ifptr->ifa_addr->sa_family != AF_INET)
            continue;

        switch (ifptr->ifa_addr->sa_family) {
            case AF_INET:
                addr = (struct sockaddr_in *)ifptr->ifa_addr;
                (void)inet_ntop(AF_INET, &addr->sin_addr, ip_address, sizeof(ip_address));
                break;
            case AF_INET6:
                addr6 = (struct sockaddr_in6 *)ifptr->ifa_addr;
                (void)inet_ntop(AF_INET6, &addr6->sin6_addr, ip_address, sizeof(ip_address));
                break;
            default:
                break;
        }

    }

    freeifaddrs(ifa);
}
