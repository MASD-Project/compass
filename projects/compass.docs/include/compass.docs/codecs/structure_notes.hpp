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
#ifndef COMPASS_DOCS_CODECS_STRUCTURE_NOTES_HPP
#define COMPASS_DOCS_CODECS_STRUCTURE_NOTES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's structure-notes generator (link_structure_notes.py):
 * every page a structure note links links back to it.
 *
 * A structure note is a UTF-8 page under doc/ or projects/modeling/ whose
 * #+filetags: hold structure_note. The pages its id links name before its
 * "What this cluster does not cover" and "See also" sections are its
 * cluster; one linked from a section marked "# aside" sits beside the
 * sequence. Each cluster page that is not itself a note is written, with a
 * line linking the note added to its See also section, made at its end
 * when it has none, unless it links the note already. The notes are taken
 * in ID order and a page two notes name gains both lines.
 */
extern const compass::codec::types::descriptor structure_notes;

}

#endif
