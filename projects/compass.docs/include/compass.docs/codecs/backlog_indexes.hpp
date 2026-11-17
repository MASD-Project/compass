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
#ifndef COMPASS_DOCS_CODECS_BACKLOG_INDEXES_HPP
#define COMPASS_DOCS_CODECS_BACKLOG_INDEXES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's backlog-indexes generator
 * (regenerate_backlog_indexes.py): the index page of each bucket of
 * doc/agile/product_backlog, inbox, next, deferred and discarded.
 *
 * Each page is the script's fixed head and a table of the bucket's
 * captures, the .org files directly in it and the story.org of each
 * directory in it, sorted by title without case. The table's columns are
 * padded to their widest cell, in characters.
 */
extern const compass::codec::types::descriptor backlog_indexes;

}

#endif
