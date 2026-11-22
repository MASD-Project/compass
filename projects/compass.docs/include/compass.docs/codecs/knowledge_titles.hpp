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
#ifndef COMPASS_DOCS_CODECS_KNOWLEDGE_TITLES_HPP
#define COMPASS_DOCS_CODECS_KNOWLEDGE_TITLES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief A knowledge page's title is a noun phrase, never a sentence, and
 * its file name is the title in snake case.
 *
 * Reads every document whose #+type: is knowledge, outside the paths of
 * the table Skip of the check document whose Definition names this codec.
 * Rule sentence_title: a title holding a word of the table Finite verbs,
 * or a word of the table Ambiguous when the title opens with a word of the
 * table Determiners. A page tagged with the setting skip_tag, a structure
 * note, is not held to it. Rule title_file_mismatch, unless the setting
 * title_file_mismatch is no: a file name that is not the title in snake
 * case, or a structure note's that does not end in the setting hub_suffix.
 * Rule unreadable: a check document the codec cannot read.
 */
extern const compass::codec::types::descriptor knowledge_titles;

}

#endif
