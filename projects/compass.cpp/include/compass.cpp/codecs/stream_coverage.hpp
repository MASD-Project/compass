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
#ifndef COMPASS_CPP_CODECS_STREAM_COVERAGE_HPP
#define COMPASS_CPP_CODECS_STREAM_COVERAGE_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's stream-coverage gate: every durable subject has one
 * stream, and no two streams overlap.
 *
 * Reads the .cpp and .hpp files under projects/, outside tests, ores.nats
 * and vendored or build directories, with comments blanked, and the
 * declarations in them: ensure_stream(name, covering_subjects(...))
 * creates a stream, serve_step_commands(nats, service, routes) a service's
 * step stream and its durable consumer, and js_queue_subscribe a durable
 * consumer on a named stream. Names resolve through string constants,
 * nats_subject members, arrays, make_stream_name locals and the arguments
 * callers pass. Rules: unresolved_declaration (a declaration the check
 * cannot read is a failure, not a skip), two_filter_sets,
 * unread_entry_point (the transport headers offer a call the check does
 * not read), empty_catalogue, overlapping_streams, wildcard_subject,
 * uncovered_subject, many_streams and unensured_stream, the server's own
 * two rules applied to the declarations.
 */
extern const compass::codec::types::descriptor stream_coverage;

}

#endif
