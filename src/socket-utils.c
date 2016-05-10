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
#include <errno.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <net/if.h> // if_nametoindex

#include "socket-utils.h"

int create_multicast_sender(const char* ip_address,
                            const char* port,
                            const char* interface,
                            int ttl,
                            struct sockaddr** out_addr,
                            socklen_t* out_len) {
    int sock = -1;
    struct addrinfo* info = NULL;
    struct ifaddrs* ipv4_ifs = NULL;
    int ret;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    *out_addr = NULL;

    // So this could also be used for IPv4
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    ret = getaddrinfo(ip_address, port, &hints, &info);
    if (ret != 0)
        return ret;

    *out_len = info->ai_addrlen;
    *out_addr = malloc(info->ai_addrlen);
    assert(*out_addr);
    memcpy(*out_addr, info->ai_addr, info->ai_addrlen);

    sock = socket(info->ai_family, info->ai_socktype, 0);
    if (sock == -1)
        goto errexit;

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

    if (*out_addr)
        free(*out_addr);

    if (info)
        freeaddrinfo(info);

    if (ipv4_ifs)
        freeifaddrs(ipv4_ifs);

    return -1;
}



int create_multicast_receiver(const char* ip_address,
                              const char* port,
                              const char* interface,
                              struct sockaddr** out_addr,
                              socklen_t* out_len) {
    int sock = -1;
    // The remote address we want to send to
    struct addrinfo* remote_address = NULL;
    // The local address we'll get bound to
    struct addrinfo* local_address = NULL;
    struct ifaddrs* ipv4_ifs = NULL;
    int ret;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    *out_addr = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    ret = getaddrinfo(ip_address, port, &hints, &remote_address);
    if (ret != 0)
        return ret;

    *out_addr = malloc(remote_address->ai_addrlen);
    *out_len = remote_address->ai_addrlen;
    assert(*out_addr);
    memcpy(*out_addr, remote_address->ai_addr, remote_address->ai_addrlen);

    sock = socket(remote_address->ai_family, remote_address->ai_socktype, 0);
    if (sock == -1)
        goto errexit;

    int yes = 1;
    ret = setsockopt(sock,
                     SOL_SOCKET,
                     SO_REUSEADDR,
                     &yes,
                     sizeof(yes));
    if (ret != 0)
        goto errexit;

    hints.ai_family = remote_address->ai_family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // Ensure we can bind to it

    ret = getaddrinfo(NULL, port, &hints, &local_address);
    if (ret != 0)
        goto errexit;

    ret = bind(sock, local_address->ai_addr, local_address->ai_addrlen);
    if (ret != 0)
        goto errexit;


    // IPv4
    if (remote_address->ai_family  == AF_INET) {
        assert(remote_address->ai_addrlen == sizeof(struct sockaddr_in));
        struct ip_mreq request;

        memcpy(&request.imr_multiaddr,
               &((struct sockaddr_in*)remote_address->ai_addr)->sin_addr,
               sizeof(request.imr_multiaddr));

        request.imr_interface.s_addr = htonl(INADDR_ANY);

        if (interface) {
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

                struct sockaddr_in* ifa_addr = (struct sockaddr_in*)current->ifa_addr;
                memcpy(&request.imr_interface,
                       &ifa_addr->sin_addr,
                       sizeof(request.imr_interface));

                ret = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &request, sizeof(request));
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
        } else {
            ret = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &request, sizeof(request));
            if (ret != 0)
                goto errexit;
        }
    } else if (remote_address->ai_family  == AF_INET6) {
        assert(remote_address->ai_addrlen == sizeof(struct sockaddr_in6));
        struct ipv6_mreq request;

        memcpy(&request.ipv6mr_multiaddr,
               &((struct sockaddr_in6*)remote_address->ai_addr)->sin6_addr,
               sizeof(request.ipv6mr_multiaddr));

        request.ipv6mr_interface = 0;

        if (interface) {
            unsigned int interface_index = if_nametoindex(interface);
            if (!interface_index)
                goto errexit;
            request.ipv6mr_interface = interface_index;
        }

        ret = setsockopt(sock,
                         IPPROTO_IPV6,
                         IPV6_ADD_MEMBERSHIP,
                         &request,
                         sizeof(request));
        if (ret != 0)
            goto errexit;
    } else {
        assert(!"Not ipv4 nor ipv6?");
    }

    freeaddrinfo(remote_address);
    freeaddrinfo(local_address);
    if (ipv4_ifs)
        freeifaddrs(ipv4_ifs);

    assert(sock != -1);
    return sock;

errexit:
    if (sock != -1)
        close(sock);

    if (*out_addr)
        free(*out_addr);

    if (remote_address)
        freeaddrinfo(remote_address);

    if (local_address)
        freeaddrinfo(local_address);

    if (ipv4_ifs)
        freeifaddrs(ipv4_ifs);

    return -1;
}
