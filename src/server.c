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
#include <signal.h>
#include <netdb.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <sys/wait.h>

#include "logger.h"
#include "config.h"
#include "socket-utils.h"

void show_usage(int _argc, char** argv) {
    fprintf(stderr, "Usage: %s [options]\n", argv[0]);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h, --help\t Display this message and exit\n");
    fprintf(stderr, "  -a, --address [address]\t IPv6 address\n");
    fprintf(stderr, "  -i, --interface [iface]\t network interface\n");
    fprintf(stderr, "  --ttl [ttl] \t Time to live\n");
    fprintf(stderr, "  -d, --daemonize \t Make the process a daemon\n");
    fprintf(stderr, "  -p, --port [port]\t Listen to [port]\n");
    fprintf(stderr, "  -v, --verbose\t Be verbose about what is going on\n");
    fprintf(stderr, "  -l, --log [file]\t Log to [file]\n");
    fprintf(stderr, "  -f, --file [file]\t Use [file] as event data source\n");
    fprintf(stderr, "  --disable-loopback \t Disable loopback\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Author(s):\n");
    fprintf(stderr, "  Emilio Cobos Álvarez (<emiliocobos@usal.es>)\n");
}

typedef enum daemon_action {
    DAEMON_ACTION_REBUILD,
    DAEMON_ACTION_CONTINUE,
    DAEMON_ACTION_EXIT,
} daemon_action_t;

const int HANDLED_SIGNALS[] = { SIGINT, SIGHUP, SIGALRM, SIGTERM };
#define HANDLED_SIGNALS_COUNT (sizeof(HANDLED_SIGNALS) / sizeof(*HANDLED_SIGNALS))

void daemonize_sig_handler(int signal) {
    switch (signal) {
        case SIGCHLD: // The daemon died
            FATAL("Daemon has died on startup");
        case SIGALRM:
            LOG("Daemon timed out waiting for children, they're probably fine");
            exit(0);
    }
}

void setup_signal_handlers() {
    sigset_t set;
    sigemptyset(&set);

    for (size_t i = 0; i < HANDLED_SIGNALS_COUNT; ++i)
        sigaddset(&set, HANDLED_SIGNALS[i]);

    int ret = sigprocmask(SIG_BLOCK, &set, NULL);
    assert(ret == 0);
}

void cancel_all_threads(pthread_t* threads,
                        bool* thread_statuses,
                        size_t length) {
    void* ret;
    for (size_t i = 0; i < length; ++i) {
        if (!thread_statuses[i])
            continue;

        if (pthread_kill(threads[i], 0) == 0) {
            LOG("Running thread %zu, cancelling", i);
            pthread_cancel(threads[i]);
        }
        pthread_join(threads[i], &ret);
        LOG("Joined thread %zu, canceled: %s",
                i, ret == PTHREAD_CANCELED ? "yes" : "no");
        thread_statuses[i] = false;
    }
}

daemon_action_t wait_and_cleanup(event_list_t* list,
                                 pthread_t* threads,
                                 bool* thread_statuses) {
    sigset_t set;
    sigemptyset(&set);

    for (size_t i = 0; i < HANDLED_SIGNALS_COUNT; ++i)
        sigaddset(&set, HANDLED_SIGNALS[i]);

    // 1 second timeout for cleaning up.
    //
    // TODO: This shouldn't (but could) interfere with sleep() calls in other
    // threads. After a few runs it seems it doesn't, but it could in other
    // systems perhaps? Be sure about it.
    alarm(1);
    int sig;
    int ret = sigwait(&set, &sig);
    assert(ret == 0);

    size_t length = event_list_size(list);
    size_t i;
    switch (sig) {
        case SIGINT:
        case SIGTERM:
            WARN("Got interrupt signal, exiting...");
            cancel_all_threads(threads, thread_statuses, length);
            return DAEMON_ACTION_EXIT;
        case SIGALRM:
            LOG("Alarm, cleaning up possibly exited threads");
            for (i = 0; i < length; ++i) {
                if (!thread_statuses[i])
                    continue;

                if (pthread_kill(threads[i], 0) != 0) {
                    LOG("Cleaning up exiting thread %zu", i);
                    pthread_join(threads[i], NULL);
                    thread_statuses[i] = false;
                }
            }

            return DAEMON_ACTION_CONTINUE;
        case SIGHUP:
            LOG("Got hangup signal, trying to rebuild configuration...");
            cancel_all_threads(threads, thread_statuses, length);
            return DAEMON_ACTION_REBUILD;
        default:
            assert(!"Invalid signal caught?");
    }
}

typedef struct dispatcher_data {
    int socket;
    event_t event;
    pthread_mutex_t* socket_mutex;
    struct sockaddr* addr;
    socklen_t addr_len;
} dispatcher_data_t;

void* event_dispatcher(void* arg) {
    // Copy into the stack our heap data to free it and prevent the leak if this
    // thread is cancelled.
    //
    // NOTE: Is important that this function NEVER explicitely allocates,
    // in order for it to be killed safely from the outside without leaking.
    dispatcher_data_t* heap_data = (dispatcher_data_t*) arg;
    dispatcher_data_t data = *heap_data;
    free(heap_data);


    time_t initial = time(NULL);
    time_t current_time;

    size_t description_length = strlen(data.event.description);

    // NB: We have to call pthread_setcancelstate(DISABLE) before the lock and
    // ENABLE afterwards, because `sendto()` is required to be a cancellation
    // point[1], so we could leave the mutex in a bad state (don't know how
    // pthread handles this internally, but better be safe than sorry).
    //
    // [1]: http://man7.org/linux/man-pages/man7/pthreads.7.html
    do {
        pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        pthread_mutex_lock(data.socket_mutex);
        int ret = sendto(data.socket,
                         data.event.description,
                         description_length + 1, 0,
                         data.addr,
                         data.addr_len);
        pthread_mutex_unlock(data.socket_mutex);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

        if (ret < 0)
            FATAL("send: %s", strerror(errno));

        LOG("dispatch: %s (%ld, %ld)", data.event.description,
                                       data.event.repeat_during,
                                       data.event.repeat_after);


        // TODO: Sleep is not super-accurate, but it's probably close enough for
        // this, and I don't think we're expected to do something more
        // sophisticated.
        sleep(data.event.repeat_after);
        current_time = time(NULL);
    } while (!data.event.repeat_during ||
             current_time - initial < data.event.repeat_during);

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
int create_dispatchers(int socket,
                       const char* events_src_filename,
                       struct sockaddr* addr,
                       socklen_t len) {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    event_list_t list = EVENT_LIST_INITIALIZER;
    pthread_t* threads = NULL;
    bool* statuses = NULL;
    daemon_action_t next_action = DAEMON_ACTION_REBUILD;

    while (next_action != DAEMON_ACTION_EXIT) {
        if (next_action == DAEMON_ACTION_REBUILD) {

            // Only freeing the heap memory, the cleanup function takes care of
            // terminating them.
            if (threads)
                free(threads);

            if (statuses)
                free(statuses);

            event_list_destroy(&list);

            if (!parse_config_file(events_src_filename, &list))
                WARN("Failed to parse config file, continuing with empty list");

            if (event_list_is_empty(&list)) {
                threads = NULL;
                statuses = NULL;
            } else {
                threads = malloc(sizeof(pthread_t) * event_list_size(&list));
                statuses = malloc(sizeof(bool) * event_list_size(&list));
                assert(threads);
                assert(statuses);
            }

            size_t length = event_list_size(&list);
            size_t index = 0;

            event_list_node_t* current = event_list_head(&list);
            while (event_list_node_has_value(current)) {
                dispatcher_data_t* data = malloc(sizeof(dispatcher_data_t));
                assert(data);
                event_t* event = event_list_node_value(current);

                data->socket = socket;
                data->event = *event;
                data->socket_mutex = &mutex;
                data->addr = addr;
                data->addr_len = len;

                statuses[index] = true;
                int result = pthread_create(threads + index, NULL, event_dispatcher, data);
                if (result != 0)
                    FATAL("Unable to create thread to dispatch event: %s", event->description);

                index++;
                current = event_list_node_next(current);
            }

            assert(index == length);
        } // DAEMON_ACTION_REBUILD

        next_action = wait_and_cleanup(&list, threads, statuses);
    }

    LOG("Terminating");
    close(socket);

    event_list_destroy(&list);

    if (threads)
        free(threads);

    if (statuses)
        free(statuses);

    return 0;
}

int main(int argc, char** argv) {
    const char* events_src_filename = "etc/events.txt";
    const char* ip_address = "ff02:0:0:0:2:3:2:4";
    const char* interface = NULL;
    const char* port = "8000";
    int ttl = 1;
    bool daemonize = false;
    bool enable_loopback = true;

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
            port = argv[i];
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
        } else if (strcmp(argv[i], "-d") == 0 ||
                   strcmp(argv[i], "--daemonize") == 0) {
            daemonize = true;
        } else if (strcmp(argv[i], "--disable-loopback") == 0) {
            enable_loopback = false;
        } else if (strcmp(argv[i], "-i") == 0 ||
                   strcmp(argv[i], "--interface") == 0) {
            ++i;
            if (i == argc)
                FATAL("The %s option needs a value", argv[i - 1]);
            interface = argv[i];
        } else if (strcmp(argv[i], "--ttl") == 0) {
            ++i;
            if (i == argc || argv[i][0] < '0' || argv[i][0] > '9')
                FATAL("The %s option needs a numeric value", argv[i - 1]);
            ttl = atoi(argv[i]);
        } else {
            WARN("Unhandled option: %s", argv[i]);
        }
    }

    LOG("events: %s", events_src_filename);
    LOG("iface: %s, ip: %s, port: %s daemonize: %s, ttl: %d, loopback: %s",
        interface, ip_address, port, daemonize ? "y" : "n", ttl,
        enable_loopback ? "y" : "n");

    if (daemonize) {
        if (signal(SIGCHLD, daemonize_sig_handler) == SIG_ERR)
            FATAL("Error registering SIGCHLD: %s", strerror(errno));

        if (signal(SIGALRM, daemonize_sig_handler) == SIG_ERR)
            FATAL("Error registering SIGALRM: %s", strerror(errno));

        pid_t child_pid;
        switch ((child_pid = fork())) {
        case 0:
            if (signal(SIGALRM, SIG_IGN) == SIG_ERR)
                FATAL("Error ignoring SIGHUP: %s", strerror(errno));

            if (signal(SIGCHLD, SIG_IGN) == SIG_ERR)
                FATAL("Error ignoring SIGCHLD: %s", strerror(errno));

            setup_signal_handlers();
            break;
        case -1:
            FATAL("Fork error: %s", strerror(errno));
            break;
        default:
            // NOTE: this is an educated guess, and does not guarantee
            // that the server is fine. We assume that if after two seconds
            // it's fine, it will be fine forever, but this does not have to
            // be true.
            alarm(2);
            waitpid(child_pid, NULL, 0); // Either SIGCHLD (if the daemon
                                         // dies) or SIGALRM will arrive
            return 0;
        }
    } else {
        // If we don't want to make this a daemon, we setup the normal signal
        // handlers and we're done.
        setup_signal_handlers();
    }

    struct sockaddr* addr;
    socklen_t len;
    int socket = create_multicast_sender(ip_address, port,
                                         interface, ttl,
                                         enable_loopback,
                                         &addr, &len);
    if (socket < 0)
        FATAL("Error creating sender (%d, %d): %s", socket, errno,
                                                    errno ? strerror(errno)
                                                          : gai_strerror(socket));

    int ret = create_dispatchers(socket, events_src_filename, addr, len);

    if (LOGGER_CONFIG.log_file)
        fclose(LOGGER_CONFIG.log_file);

    free(addr);
    return ret;
}
