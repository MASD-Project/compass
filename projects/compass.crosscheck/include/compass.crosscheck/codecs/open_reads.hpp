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
#ifndef COMPASS_CROSSCHECK_CODECS_OPEN_READS_HPP
#define COMPASS_CROSSCHECK_CODECS_OPEN_READS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's open-reads gate: every read with no permission
 * check is allow-listed.
 *
 * A read is a method of a generated core messaging handler whose name
 * starts with list_ or get_ or that decodes a protocol request; it is open
 * when its body holds no has_permission(. Its subject is the one its
 * comment says it serves, else each decoded request's. Rule open_read: an
 * open read whose subject the allow_list page's "** The allow-list" table
 * does not name. Rule unguarded_history: a generated history registrar
 * whose registrations do not each name a literal read code. Rule
 * stale_allow: an allowed subject no protocol header defines. Rule
 * no_allow_list: the page has no allow-list section.
 */
extern const compass::codec::types::descriptor open_reads;

}

#endif
