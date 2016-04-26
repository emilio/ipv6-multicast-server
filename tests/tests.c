/**
 * tests.c:
 *   Unit tests for the package
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
#include <string.h>

#include "tests.h"
#include "event.h"
#include "config.h"

event_list_t mock_list(size_t event_count) {
    event_list_t list = EVENT_LIST_INITIALIZER;

    event_t event = EVENT_INITIALIZER;
    for (size_t i = 0; i < event_count; ++i) {
        event.repeat_after = i;
        event.repeat_during = i;
        event_list_push(&list, &event);
    }

    return list;
}

TEST(event_list_push_pop, {
    event_list_t list = EVENT_LIST_INITIALIZER;
    event_t event = EVENT_INITIALIZER;
    event.repeat_after = 10;
    event.repeat_during = 100;

    event_list_push(&list, &event);

    event_t new_event;
    event_list_pop(&list, &new_event);
    ASSERT(new_event.repeat_after == event.repeat_after);
    ASSERT(new_event.repeat_during == event.repeat_during);
    ASSERT(event_list_size(&list) == 0);
})

TEST(event_list_del_middle, {
    event_list_t list = mock_list(5);

    event_list_node_t* current = event_list_head(&list);
    size_t i = 0;
    while (event_list_node_has_value(current)) {
        event_t event = event_list_node_value(current);
        ASSERT(event.repeat_after == i);
        ASSERT(event.repeat_during == i);

        if (i == 2) {
            event_list_remove(&list, current);
        } else {
            current = event_list_node_next(current);
        }
        ++i;
    }

    ASSERT(event_list_size(&list) == 4);

    event_list_destroy(&list);
})

TEST(event_parsing, {
    event_t event;

    ASSERT(parse_event("1 2 abc", &event));

    ASSERT(event.repeat_after == 1);
    ASSERT(event.repeat_during == 2);
    ASSERT(strcmp(event.description, "abc") == 0);
})

TEST_MAIN({
    RUN_TEST(event_list_push_pop);
    RUN_TEST(event_list_del_middle);
    RUN_TEST(event_parsing);
})
