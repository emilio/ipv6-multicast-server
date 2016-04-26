/**
 * event.h:
 *   Base event definition
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
#ifndef EVENT_H_
#define EVENT_H_

#include <stdbool.h>
#include <time.h>

#define MAX_EVENT_DESCRIPTION_SIZE 255

/**
 * The server broadcasts events each `repeat_after`
 * seconds for `repeat_during` seconds.
 *
 * Each event contains an **owned** string, guaranteed
 * to be null-terminated, or NULL if there's no description.
 */
typedef struct event {
    time_t repeat_after;
    time_t repeat_during;
    char description[MAX_EVENT_DESCRIPTION_SIZE];
} event_t;

#define EVENT_INITIALIZER {0, 0, {0}}

typedef struct event_list_node {
    event_t event;
    struct event_list_node* next;
} event_list_node_t;

/**
 * Singly linked list with fake head to ease deletion of events.
 */
typedef struct event_list {
    event_list_node_t* head;
    event_list_node_t* tail;
    size_t size;
} event_list_t;

#define EVENT_LIST_INITIALIZER {NULL, NULL, 0}

#define event_list_node_value(n) (&(n)->next->event)
#define event_list_head(l) ((l)->head)
#define event_list_last(l) ((l)->last)
#define event_list_size(l) ((l)->size)

#define event_list_node_next(n) ((n)->next)
#define event_list_node_has_value(n) ((n) && (n)->next)
/**
 * Since we use a fake head, we don't just have to check for ->next,
 * but also for ->next->next
 */
inline static
bool event_list_node_is_empty(event_list_node_t* node) {
    return !node || !node->next;
}

void event_list_push(event_list_t* l, event_t* event);
bool event_list_pop(event_list_t* l, event_t* out_event);
void event_list_remove(event_list_t* l, event_list_node_t* node);
void event_list_destroy(event_list_t* l);

#endif
