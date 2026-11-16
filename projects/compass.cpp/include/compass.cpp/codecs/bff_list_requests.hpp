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
#ifndef COMPASS_CPP_CODECS_BFF_LIST_REQUESTS_HPP
#define COMPASS_CPP_CODECS_BFF_LIST_REQUESTS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's bff-list-requests gate: every hand-written list
 * request states what the protocol decodes.
 *
 * Reads the structs of every
 * projects/<component>/api/include/<name>/messaging/<entity>_protocol.hpp
 * and the .ts files directly under projects/ores.web/packages/bff/src, not the tests. A
 * list_*_request passed to a call with an object literal that states a
 * request key, outside a readAll call: rule missing_member when it leaves
 * out as_of or the filter the request declares; rule filter_mismatch when
 * a literal filter states other members than the filter struct. Rule
 * wrong_rows: readAll reads a rows key the reply does not carry. Rule
 * stale_table: the wire-protocol list_requests.ts is not the table the
 * headers give. prettier is not run: an array goes on one line when it
 * fits the 100 columns .prettierrc.yaml sets, as prettier lays it out.
 */
extern const compass::codec::types::descriptor bff_list_requests;

}

#endif
