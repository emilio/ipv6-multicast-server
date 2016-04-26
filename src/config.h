/**
 * config.h:
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
#include <stdbool.h>
#include "event.h"

/**
 * The config file consist of multiple lines like:
 *
 * ```
 * repeat_after repeat_until description
 * ```
 *
 * If repeat_after or repeat_until is zero, it never repeats.
 *
 * We return a linked list of event_t elements in out_list.
 */
bool read_long(const char** cursor, long* result);

bool parse_config_file(const char* filename, event_list_t* out_list);

bool parse_event(const char* str, event_t* event);
