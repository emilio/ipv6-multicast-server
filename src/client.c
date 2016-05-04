/**
 * client.c:
 *   Main client logic
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>

#include "logger.h"
#include "socket-utils.h"

/// Shows usage of the program
void show_usage(int _argc, char** argv) {
    fprintf(stderr, "Usage: %s [options]\n", argv[0]);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help\t Display this message and exit\n");
    fprintf(stderr, "  -a, --address [address]\t IPv6 address\n");
    fprintf(stderr, "  -i, --interface [iface]\t network interface\n");
    fprintf(stderr, "  -p, --port [port]\t Listen to [port]\n");
    fprintf(stderr, "  -v, --verbose\t Be verbose about what is going on\n");
    fprintf(stderr, "  -l, --log [file]\t Log to [file]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Author(s):\n");
    fprintf(stderr, "  Emilio Cobos Álvarez (<emiliocobos@usal.es>)\n");
}

void handle_interrupt(int sig) {
    exit(0);
}

int SOCKET = -1; // Yeah, global state ftw :/

void cleanly_dealloc_resources() {
    if (LOGGER_CONFIG.log_file)
        fclose(LOGGER_CONFIG.log_file);
    LOGGER_CONFIG.log_file = NULL;

    if (SOCKET != -1)
        close(SOCKET);
    SOCKET = -1;
}

int main(int argc, char** argv) {
    const char* ip_address = "ff02:0:0:0:0:0:0:f";
    const char* interface = NULL;
    const char* port = "8000";

    LOGGER_CONFIG.log_file = stderr;

    atexit(cleanly_dealloc_resources);

    signal(SIGINT, handle_interrupt);
    signal(SIGTERM, handle_interrupt);

    for(int i  = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            show_usage(argc, argv);
            return 1;
        } else if (strcmp(argv[i], "-v") == 0 ||
                   strcmp(argv[i], "--verbose") == 0) {
            LOGGER_CONFIG.verbose = true;
        } else if (strcmp(argv[i], "-l") == 0 ||
                   strcmp(argv[i], "--log") == 0) {
            ++i;
            if (i == argc)
                FATAL("The %s option needs a value", argv[i - 1]);

            FILE* log_file = fopen(argv[i], "w");
            if (log_file)
                LOGGER_CONFIG.log_file = log_file;
            else
                WARN("Could not open \"%s\", using stderr: %s", argv[i],
                     strerror(errno));
        } else if (strcmp(argv[i], "-p") == 0 ||
                   strcmp(argv[i], "--port") == 0) {
            ++i;
            if (i == argc)
                FATAL("The %s option needs a value", argv[i - 1]);
            port = argv[i];
        } else if (strcmp(argv[i], "-a") == 0 ||
                   strcmp(argv[i], "--address") == 0) {
            ++i;
            if (i == argc)
                FATAL("The %s option needs a value", argv[i - 1]);
            ip_address = argv[i];
        } else if (strcmp(argv[i], "-i") == 0 ||
                   strcmp(argv[i], "--interface") == 0) {
            ++i;
            if (i == argc)
                FATAL("The %s option needs a value", argv[i - 1]);
            interface = argv[i];
        } else {
            WARN("Unhandled option: %s", argv[i]);
        }
    }

    LOG("Using iface: %s, port: %s, address: %s", interface, port, ip_address);

    struct sockaddr* addr = NULL;
    socklen_t len = 0;
    SOCKET = create_multicast_receiver(ip_address, port, interface, &addr, &len);
    if (addr)
        free(addr); // we don't care about it

    if (SOCKET < 0)
        FATAL("Error creating receiver (%d, %d): %s", SOCKET, errno,
                                                      errno ? strerror(errno)
                                                            : gai_strerror(SOCKET));

    char buffer[512];
    while (true) {
        ssize_t ret = recvfrom(SOCKET, buffer, sizeof(buffer), 0, NULL, NULL);
        if (ret < 0) {
            WARN("read error: %s", strerror(errno));
        } else {
            if (ret < sizeof(buffer))
                buffer[ret] = '\0';
            else
                buffer[sizeof(buffer) - 1] = '\0';
            printf("> %s\n", buffer);
        }
    }

    assert(!"Unreachable");

    return 0;
}
