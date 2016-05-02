/**
 * server.c:
 *   UDP server's logic
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
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <signal.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#include <assert.h>

#include "logger.h"
#include "config.h"

void show_usage(int _argc, char** argv) {
    fprintf(stderr, "Usage: %s [options]\n", argv[0]);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help\t Display this message and exit\n");
    fprintf(stderr, "  -p, --port [port]\t Listen to [port]\n");
    fprintf(stderr, "  -v, --verbose\t Be verbose about what is going on\n");
    fprintf(stderr, "  -l, --log [file]\t Log to [file]\n");
    fprintf(stderr, "  -f, --file [file]\t Use [file] as event data source\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Author(s):\n");
    fprintf(stderr, "  Emilio Cobos Álvarez (<emiliocobos@usal.es>)\n");
}

bool list_is_ordered(event_list_t* list) {
    if (event_list_is_empty(list))
        return true;

    event_list_node_t* current = event_list_head(list);

    assert(event_list_node_has_value(current));

    time_t last = event_list_node_value(current)->repeat_after;
    current = event_list_node_next(current);

    while (event_list_node_has_value(current)) {
        event_t* event = event_list_node_value(current);
        if (event->repeat_after < last)
            return false;

        last = event->repeat_after;
        current = event_list_node_next(current);
    }

    return true;
}

int run_event_loop(event_list_t* list) {
    assert(list);
    assert(list_is_ordered(list));

    event_list_node_t* current = event_list_head(list);
    while (event_list_node_has_value(current)) {
        event_t* event = event_list_node_value(current);
        printf("%s: %ld %ld\n", event->description, event->repeat_during,
                event->repeat_after);

        current = event_list_node_next(current);
    }

    // TODO: Dispatch the events.

    return 0;
}

int main(int argc, char** argv) {
    const char* events_src_filename = "etc/events.txt";
    long port = 8000;

    LOGGER_CONFIG.log_file = stderr;

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
            const char* cursor = argv[i];
            if (!read_long(&cursor, &port))
                WARN("Using default port %ld", port);
        } else if (strcmp(argv[i], "-f") == 0 ||
                   strcmp(argv[i], "--file") == 0) {
            ++i;
            if (i == argc)
                FATAL("The %s option needs a value", argv[i - 1]);
            events_src_filename = argv[i];
        } else {
            WARN("Unhandled option: %s", argv[i]);
        }
    }

    LOG("events: %s", events_src_filename);
    event_list_t list = EVENT_LIST_INITIALIZER;

    if (!parse_config_file(events_src_filename, &list))
        FATAL("Failed to parse config, aborting");

    return run_event_loop(&list);
}
