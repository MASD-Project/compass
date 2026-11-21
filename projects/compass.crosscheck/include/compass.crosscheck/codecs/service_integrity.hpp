/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#ifndef COMPASS_CROSSCHECK_CODECS_SERVICE_INTEGRITY_HPP
#define COMPASS_CROSSCHECK_CODECS_SERVICE_INTEGRITY_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief Whether every service of a project is whole: registered, and
 * meeting each obligation the project lists for a service.
 *
 * The service registry is the root: every document whose #+type: is
 * ores.codegen.service_registry, loaded as the model loads it, so an entry
 * is a top-level heading with :replicas:. Each entry is a service with the
 * fields its patterns and conditions name:
 *
 * - name, the heading's title;
 * - runtime, its :runtime:, or the setting default_runtime when unset;
 * - enabled, "true" or "false", as the model reads :enabled:;
 * - part, the directory of its service part, when it has one;
 * - every key of its property drawer, with its value.
 *
 * A service part is a component overview (#+type: ores.codegen.component)
 * whose #+component_kind: is the setting part_kind. Its #+full_name: names
 * it, and its directory is the one above the modeling directory the
 * overview sits in.
 *
 * The check document whose Definition names this codec gives the data as
 * tables, each under the level-1 heading of its name:
 *
 * - Settings (setting, value): part_kind, the #+component_kind: of a
 *   service part; default_runtime, the runtime of an entry with no
 *   :runtime:; single_source_minimum, how many service names make a list.
 * - Runtimes (runtime, has part): every runtime an entry may have, and
 *   "yes" when an entry of it is built from a service part, else "no".
 * - Obligations (obligation, test, path, value, condition, reason): one
 *   row per thing a service must satisfy. obligation names it, and the
 *   finding carries the name. test is one of:
 *   - exists: the file at path exists;
 *   - contains: the file at path holds value;
 *   - has_line: a line of the file at path, without its blanks at either
 *     end, is value;
 *   - lacks: the file at path, when it exists, does not hold value.
 *   path and value are patterns: {field} is a field of the service, and
 *   {field[i]} the i-th, from 0, of its pieces split at '.', so
 *   {name[1]} of ores.iam.service is iam. condition is words, all of
 *   which must hold: a field that is set and not "false", or !field for
 *   one that is not. An obligation applies to a service its condition
 *   holds for and that has every field its patterns name. A field that no
 *   entry has and that is not one of the four above is unreadable.
 * - Single source files (glob, reason): the files that must not list the
 *   services, and Single source exceptions (glob, reason): those excused,
 *   such as tests and generated outputs. A glob without '/' matches a
 *   file's name, one with '/' its path; '*' and '?' stay within a
 *   segment and '**' spans segments.
 *
 * The rules: every service part has an entry (unregistered); every entry
 * whose runtime has parts names a part that exists (partless); every
 * entry's runtime is a row of Runtimes (unknown_runtime); every service
 * meets each obligation that applies to it (unmet_obligation), found at
 * its entry; and no scanned file other than the registry holds a literal
 * list of single_source_minimum or more distinct registry names,
 * separated only by blanks, commas, quotes, brackets, semicolons and
 * backslashes (second_list). A missing document, table, column or
 * setting, or a row that does not read, is unreadable and checks nothing.
 */
extern const compass::codec::types::descriptor service_integrity;

}

#endif
