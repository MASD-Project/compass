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
#ifndef COMPASS_ORG_TYPES_TABLES_HPP
#define COMPASS_ORG_TYPES_TABLES_HPP

#include <vector>
#include "compass.org/types/affiliated_table.hpp"
#include "compass.org/types/block_table.hpp"
#include "compass.org/types/document_table.hpp"
#include "compass.org/types/element_table.hpp"
#include "compass.org/types/heading_table.hpp"
#include "compass.org/types/keyword_table.hpp"
#include "compass.org/types/link_table.hpp"
#include "compass.org/types/list_table.hpp"
#include "compass.org/types/name_table.hpp"
#include "compass.org/types/object_table.hpp"
#include "compass.org/types/property_table.hpp"

namespace compass::org::types {

/**
 * @brief Everything org documents become: one document, or a corpus of them
 * end to end.
 *
 * The source text is not here. Every source_range is read against the bytes
 * of its document, which the caller keeps mapped. documents is empty for a
 * single document and has one row per document for a corpus.
 */
struct tables {
    element_table elements;
    object_table objects;
    heading_table headings;
    heading_tag_table heading_tags;
    keyword_table keywords;
    property_table properties;
    affiliated_table affiliated;
    block_table blocks;
    list_table lists;
    item_table items;
    link_table links;
    name_table names;
    std::vector<char> name_bytes;
    document_table documents;
};

}

#endif
