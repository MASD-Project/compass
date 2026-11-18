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
 * This codec reads the subjects written where no model declares them: a
 * quoted dotted name in the DQ artefact type populate script. The
 * subjects the models declare are model_rules' outside_protocol. The
 * script also read C++ files with a pattern that opens on a double quote
 * and closes on a single one, so it matched nothing; that half is not
 * ported. Rule outside_protocol: a subject that is not
 * component.v1.resource.verb with a verb of the closed set or list_by_,
 * component.v1.ops.operation, or component.v1.events.collection.action.
 */
extern const compass::codec::types::descriptor subject_conformance;

}

#endif
