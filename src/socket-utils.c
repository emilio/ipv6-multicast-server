/**
 * socket-utils.c:
 *   Socket comparison utils
 *
 * Copyright (C) 2015 Emilio Cobos Álvarez (70912324N) <emiliocobos@usal.es>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "socket-utils.h"

#include <net/if.h>
#include <stdbool.h>
#include <string.h>
#include <ifaddrs.h>
#include <errno.h>

#ifdef SOCK_UN
int sockaddr_un_cmp(struct sockaddr_un* x, struct sockaddr_un* y) {
    return strcmp(x->sun_path, y->sun_path);
}
#endif

#ifdef SOCK_IN6
int sockaddr_in6_cmp(struct sockaddr_in6* x, struct sockaddr_in6* y) {
    if (x->sin6_port != y->sin6_port)
        return x->sin6_port - y->sin6_port;

    if (x->sin6_flowinfo != y->sin6_flowinfo)
        return x->sin6_flowinfo - y->sin6_flowinfo;

    if (x->sin6_scope_id != y->sin6_scope_id)
        return x->sin6_scope_id - y->sin6_scope_id;

    return memcmp(x->sin6_addr.s6_addr, y->sin6_addr.s6_addr,
                  sizeof(x->sin6_addr.s6_addr));
}
#endif

int sockaddr_in_cmp(struct sockaddr_in* x, struct sockaddr_in* y) {
    if (x->sin_addr.s_addr != y->sin_addr.s_addr)
        return x->sin_addr.s_addr - y->sin_addr.s_addr;

    return x->sin_port - y->sin_port;
}

int sockaddr_cmp(struct sockaddr* x, struct sockaddr* y) {
    if (x->sa_family != y->sa_family)
        return x->sa_family - y->sa_family;

    switch (x->sa_family) {
#ifdef SOCK_UN
        case AF_UNIX:
            return sockaddr_un_cmp(SOCKADDR_UN_PTR(x), SOCKADDR_UN_PTR(y));
#endif
        case AF_INET:
            return sockaddr_in_cmp(SOCKADDR_IN_PTR(x), SOCKADDR_IN_PTR(y));
#ifdef SOCK_IN6
        case AF_INET6:
            return sockaddr_in6_cmp(SOCKADDR_IN6_PTR(x), SOCKADDR_IN6_PTR(y));
#endif
        default:
            assert(0 && "Unknown sockaddr family");
    }
    return 0;
}

int create_multicast_sender(const char* ip_address,
                            const char* port,
                            const char* interface,
                            int ttl,
                            struct sockaddr* out_addr,
                            socklen_t* out_len) {
    struct addrinfo hints;
    int sock = -1;
    struct addrinfo* info = NULL;
    struct ifaddrs* ipv4_ifs = NULL;
    int ret;

    memset(&hints, 0, sizeof(hints));

    // So this could also be used for IPv4
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    ret = getaddrinfo(ip_address, port, &hints, &info);
    if (ret != 0)
        return ret;

    *out_len = info->ai_addrlen;
    memcpy(out_addr, info->ai_addr, info->ai_addrlen);

    sock = socket(info->ai_family, info->ai_socktype, 0);
    if (sock == -1)
        return -1;

    ret = setsockopt(sock,
                     info->ai_family == AF_INET6 ? IPPROTO_IPV6
                                                 : IPPROTO_IP,
                     info->ai_family == AF_INET6 ? IPV6_MULTICAST_HOPS
                                                 : IP_MULTICAST_TTL,
                     &ttl,
                     sizeof(ttl));
    if (ret != 0)
        goto errexit;

    if (info->ai_family == AF_INET && interface) {
        ret = getifaddrs(&ipv4_ifs);
        if (ret != 0)
            goto errexit;

        struct ifaddrs* current = ipv4_ifs;
        bool found = false;
        for (; current; current = current->ifa_next) {
            if (current->ifa_addr->sa_family != AF_INET)
                continue;

            if (strcmp(current->ifa_name, interface) != 0)
                continue;


            struct sockaddr_in* addr = (struct sockaddr_in*)current->ifa_addr;

            ret = setsockopt(sock,
                             IPPROTO_IP,
                             IP_MULTICAST_IF,
                             &addr->sin_addr,
                             sizeof(struct in_addr));
            if (ret == 0) {
                found = true;
                break;
            }
        }

        if (!found) {
            if (!errno)
                errno = EINVAL;
            goto errexit;
        }
    } else if (info->ai_family == AF_INET6 && interface) {
        unsigned int interface_index = if_nametoindex(interface);
        if (!interface_index)
            goto errexit;

        ret = setsockopt(sock,
                         IPPROTO_IPV6,
                         IPV6_MULTICAST_IF,
                         &interface_index,
                         sizeof(interface_index));
        if (ret != 0)
            goto errexit;
    }

    freeaddrinfo(info);
    if (ipv4_ifs)
        freeifaddrs(ipv4_ifs);

    assert(sock != -1);
    return sock;

errexit:
    if (sock != -1)
        close(sock);

    if (info)
        freeaddrinfo(info);

    if (ipv4_ifs)
        freeifaddrs(ipv4_ifs);

    return -1;
}
