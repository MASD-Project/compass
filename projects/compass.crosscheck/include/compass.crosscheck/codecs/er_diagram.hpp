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
#ifndef COMPASS_CROSSCHECK_CODECS_ER_DIAGRAM_HPP
#define COMPASS_CROSSCHECK_CODECS_ER_DIAGRAM_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's er-diagram generator (plantuml_er_parse_sql.py, then
 * plantuml_er_generate.py): one PlantUML diagram per table group and the
 * index page that names them, from the create scripts.
 *
 * The scripts are read by compass.sql's er_model_of. Its tables,
 * COMPONENT_PREFIXES, FK_COLUMNS and TABLE_STYLES, are read from the
 * parser's source (option parser); the diagrams are rendered from the
 * templates (options template and index_template) as pystache renders
 * them, without escaping. Where the parser's tables cannot be read, the
 * codec writes nothing. The parser's convention warnings are not ported.
 */
extern const compass::codec::types::descriptor er_diagram;

}

#endif
