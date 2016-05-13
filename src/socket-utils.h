/**
 * soket-utils.h:
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
#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H
#include <arpa/inet.h>

int create_multicast_sender(const char* ip_address,
                            const char* port,
                            const char* interface,
                            int ttl,
                            bool enable_loopback,
                            struct sockaddr** out_addr,
                            socklen_t* out_len);

int create_multicast_receiver(const char* ip_address,
                              const char* port,
                              const char* interface,
                              struct sockaddr** out_addr,
                              socklen_t* out_len);
#endif
