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
#ifndef COMPASS_CODEC_CODECS_DOCUMENT_INDEX_HPP
#define COMPASS_CODEC_CODECS_DOCUMENT_INDEX_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::codec::codecs {

/**
 * @brief Emits one file that lists every document of the corpus.
 *
 * The file is tab-separated text: a header line "id", "title", "type",
 * "path", then one line per document, in byte order of the paths. id is
 * the ID property of the document, title its #+title: and type its
 * #+type: as written, each empty when the document has none. A tab in a
 * field is written as a space. The option "path" names the file, relative
 * to the root of the checkout.
 */
extern const types::descriptor document_index;

}

#endif
