/**
 * event.c:
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
#include "event.h"
#include <assert.h>
#include <stdlib.h>

static inline
event_list_node_t*
new_node(event_t* event) {
    event_list_node_t* ret = malloc(sizeof(event_list_node_t));
    assert(ret);
    ret->event = *event;
    ret->next = NULL;
    return ret;
}

static inline
void node_free(event_list_node_t** node) {
    assert(node && *node);
    free(*node);
    *node = NULL;
}

static inline
event_list_node_t* make_dummy_list_node() {
    event_t dummy = EVENT_INITIALIZER;
    return new_node(&dummy);
}


void event_list_push(event_list_t* l, event_t* event) {
    assert(event);
    if (l->tail == NULL) {
        assert(l->head == NULL);
        l->head = make_dummy_list_node();
        l->tail = NULL;
    }
    event_list_node_t* new = new_node(event);

    if (l->tail == NULL) {
        l->head->next = l->tail = new;
        l->size++;
        return;
    }

    assert(l->tail->next == NULL);
    l->tail->next = new;
    l->tail = new;
    l->size++;
}

bool event_list_pop(event_list_t* l, event_t* out_event) {
    // Empty list
    if (!l->head)
        return false;


    assert(l->head->next); // If not, there's a bug
    if (out_event)
        *out_event = l->head->next->event;

    event_list_remove(l, l->head);
    return true;
}

void event_list_remove(event_list_t* l, event_list_node_t* node) {
    assert(node && node->next);
    assert(l->size > 0);

    event_list_node_t* old = node->next;
    node->next = old->next;
    node_free(&old);

    if (node->next == NULL) {
        l->tail = node;
    }

    l->size--;

    // We emptied our list
    if (l->size == 0) {
        assert(l->tail == l->head);
        node_free(&l->head);
        l->tail = NULL;
    }
}
void event_list_destroy(event_list_t* l) {
    while (l->size) {
        event_list_pop(l, NULL);
    }
    assert(l->size == 0);
    assert(l->head == NULL);
    assert(l->tail == NULL);
}
