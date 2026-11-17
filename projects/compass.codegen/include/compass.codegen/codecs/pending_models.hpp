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
#ifndef COMPASS_CODEGEN_CODECS_PENDING_MODELS_HPP
#define COMPASS_CODEGEN_CODECS_PENDING_MODELS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::codegen::codecs {

/**
 * @brief ORE Studio's pending-models generator (derive_pending_models.py,
 * codegen.pending_change): the pending-change model of every gated model
 * under projects/X/modeling/ores.*.org.
 *
 * A model is gated when its first Flags section sets :gated: or
 * :pending_change: true. Its pending model, ores.C.S_change.org beside it,
 * holds the line columns and the model's own columns without keys, the
 * foreign keys to tables that have no pending change, the lookup
 * validations without their limits and the model's include blocks; a
 * junction's holds its two halves. A model the script refuses stops the
 * codec, as it stops the script.
 */
extern const compass::codec::types::descriptor pending_models;

}

#endif
