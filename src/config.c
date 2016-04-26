/**
 * config.c:
 *   Config parsing
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
#include <errno.h>
#include <string.h>
#include "config.h"
#include "logger.h"
#include "event.h"

bool read_long(const char** cursor, long* result) {
    char* endptr;
    long temp_res = strtoul(*cursor, &endptr, 0);

    if (endptr == *cursor || errno == ERANGE)
        return false;

    *result = temp_res;
    *cursor = endptr;

    return true;
}

bool read_space(const char** cursor) {
    if (**cursor != ' ')
        return false;
    (*cursor)++;
    return true;
}

bool parse_event(const char* str, event_t* event) {
    if (!event || !str) {
        return false;
    }

    const char* cursor = str;
    if (!read_long(&cursor, &event->repeat_after))
        return false;

    if (event->repeat_after < 0)
        return false;

    if (!read_space(&cursor))
        return false;

    if (!read_long(&cursor, &event->repeat_during))
        return false;

    if (event->repeat_during < 0)
        return false;

    if (!read_space(&cursor))
        return false;

    strncpy(event->description, cursor, MAX_EVENT_DESCRIPTION_SIZE);
    event->description[MAX_EVENT_DESCRIPTION_SIZE - 1] = '\0';

    return true;
}

bool parse_config_file(const char* filename, event_list_t* out_list) {
    FILE* f;
    char line[255];

    f = fopen(filename, "r");
    if (!f) {
        WARN("Couldn't open \"%s\": %s", filename, strerror(errno));
        return false;
    }

    event_t event = EVENT_INITIALIZER;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);

        // Trim last newline
        if (len && line[len - 1] == '\n')
            line[--len] = '\0';

        LOG("config_parse: %s", line);

        if (len == 0 || *line == '#') {
            LOG("config_parse: Ignoring empty line or comment");
            continue;
        }

        if (!parse_event(line, &event)) {
            WARN("Found invalid event: %s", line);
            continue;
        }

        event_list_push(out_list, &event);
    }

    return true;
}
