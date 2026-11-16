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
#ifndef COMPASS_STORE_TYPES_ACTION_KIND_HPP
#define COMPASS_STORE_TYPES_ACTION_KIND_HPP

#include <cstdint>

namespace compass::store::types {

/**
 * @brief The two kinds of action object, each in a directory of its own.
 *
 * An action object is named by a key its writer computes from what an
 * action depends on, not by the hash of its bytes. A manifest maps the key
 * of what is known before an action runs to the input sets seen for it; a
 * result holds what an action produced for one full set of inputs.
 */
enum class action_kind : std::uint8_t { manifest, result };

}

#endif
