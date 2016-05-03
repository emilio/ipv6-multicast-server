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
    fprintf(stderr, "  -a, --address [address]\t IPv6 address\n");
    fprintf(stderr, "  -p, --port [port]\t Listen to [port]\n");
    fprintf(stderr, "  -v, --verbose\t Be verbose about what is going on\n");
    fprintf(stderr, "  -l, --log [file]\t Log to [file]\n");
    fprintf(stderr, "  -f, --file [file]\t Use [file] as event data source\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Author(s):\n");
    fprintf(stderr, "  Emilio Cobos Álvarez (<emiliocobos@usal.es>)\n");
}

typedef struct dispatcher_data {
    int socket;
    event_t event;
    pthread_mutex_t* socket_mutex;
} dispatcher_data_t;

void* event_dispatcher(void* arg) {
    dispatcher_data_t* data = (dispatcher_data_t*) arg;
    time_t initial = time(NULL);
    time_t current_time;

    size_t description_length = strlen(data->event.description);
    do {
        pthread_mutex_lock(data->socket_mutex);
        int ret = send(data->socket,
                       data->event.description,
                       description_length + 1, 0);

        if (ret < 0)
            FATAL("send: %s", strerror(errno));
        pthread_mutex_unlock(data->socket_mutex);

        // TODO: Sleep is not super-accurate, but it's probably close enough for
        // this, and I don't think we're expected to do something more
        // sophisticated.
        sleep(data->event.repeat_after);
        current_time = time(NULL);
    } while (!data->event.repeat_during ||
             current_time - initial < data->event.repeat_during);

    return NULL;
}

/**
 * This function creates a thread per event and dispatchs it.
 *
 * This is **extremely** inefficient, I know, but it was a requisite stated in
 * the statement of the practice.
 *
 * The ideal way (and that's why the whole event_list exists, to allow fast
 * insertion/deletion) would be to keep a list of events sorted by when should
 * they be dispatched, and just dispatch them in order and sleep until we have
 * to dispatch the next.
 *
 * I possibly make that strategy gated by an #ifdef, but I don't have a lot of
 * time so...
 */
int create_dispatchers(int socket, event_list_t* list) {
    if (event_list_is_empty(list))
        return 0;

    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_t* threads = malloc(sizeof(pthread_t) * event_list_size(list));

    size_t length = event_list_size(list);
    size_t index = 0;

    event_list_node_t* current = event_list_head(list);
    while (event_list_node_has_value(current)) {
        dispatcher_data_t* data = malloc(sizeof(dispatcher_data_t));
        assert(data);
        event_t* event = event_list_node_value(current);

        data->socket = socket;
        data->event = *event;
        data->socket_mutex = &mutex;

        int result = pthread_create(threads + index, NULL, event_dispatcher, data);
        if (result != 0)
            FATAL("Unable to create thread to dispatch event: %s", event->description);

        index++;
        current = event_list_node_next(current);
    }

    assert(index == length);

    for (size_t i = 0; i < length; ++i)
        pthread_join(threads[i], NULL);

    return 0;
}

int create_multicast_sender(const char* ip_address, long port) {
    // TODO
    return -1;
}

int main(int argc, char** argv) {
    const char* events_src_filename = "etc/events.txt";
    const char* ip_address = "ffx1:20";
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
        } else if (strcmp(argv[i], "-a") == 0 ||
                   strcmp(argv[i], "--address") == 0) {
            ++i;
            if (i == argc)
                FATAL("The %s option needs a value", argv[i - 1]);
            ip_address = argv[i];
        } else {
            WARN("Unhandled option: %s", argv[i]);
        }
    }

    LOG("events: %s", events_src_filename);
    event_list_t list = EVENT_LIST_INITIALIZER;

    if (!parse_config_file(events_src_filename, &list))
        FATAL("Failed to parse config, aborting");

    int socket = create_multicast_sender(ip_address, port);
    return create_dispatchers(socket, &list);
}
