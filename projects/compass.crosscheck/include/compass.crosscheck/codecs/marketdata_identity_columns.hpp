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
#ifndef COMPASS_CROSSCHECK_CODECS_MARKETDATA_IDENTITY_COLUMNS_HPP
#define COMPASS_CROSSCHECK_CODECS_MARKETDATA_IDENTITY_COLUMNS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's marketdata-identity-columns gate
 * (check_marketdata_identity_columns.py): the series identity projection
 * matches the codec schema.
 *
 * Reads the codec schema (datum/schema.hpp: its field enum, identity and
 * coordinate references and per-type rows), the index grammar
 * (datum/market_index.hpp), the projector's field switch and assign_index,
 * the identity table's CREATE TABLE and the per-asset-class views. Rules
 * table_columns, projector_switch, views and index_mapping report each
 * disagreement the script reports; unreadable_source reports a source that
 * lost the shape the script reads, as the script stops on it.
 */
extern const compass::codec::types::descriptor marketdata_identity_columns;

}

#endif
