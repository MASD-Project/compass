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
#ifndef COMPASS_CROSSCHECK_CODECS_SUBJECT_CONFORMANCE_HPP
#define COMPASS_CROSSCHECK_CODECS_SUBJECT_CONFORMANCE_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's subject-conformance gate: every NATS subject is
 * inside the entity protocol.
 *
 * A subject is declared by a :subject: line of an org file directly under
 * projects/<component>/modeling/, or written where no model declares it: a
 * name holding "subject" assigned a string in a .hpp or .cpp under
 * projects/, as the script's pattern spells it (it ends at a single quote,
 * so it rarely matches), and a quoted dotted name in the DQ artefact type
 * populate script. Rule outside_protocol: a subject that is not
 * component.v1.resource.verb with a verb of the closed set or list_by_,
 * component.v1.ops.operation, or component.v1.events.collection.action.
 * Rule no_declaration: no model declares a subject. Rule unreadable_model:
 * a model that is not UTF-8. The script's baseline is replaced by inline
 * exceptions.
 */
extern const compass::codec::types::descriptor subject_conformance;

}

#endif
