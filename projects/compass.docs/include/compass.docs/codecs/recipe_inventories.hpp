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
#ifndef COMPASS_DOCS_CODECS_RECIPE_INVENTORIES_HPP
#define COMPASS_DOCS_CODECS_RECIPE_INVENTORIES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief The recipe inventories: doc/recipes/shell/shell.org and
 * doc/recipes/http/generated_recipes.org.
 *
 * The shell inventory keeps its head and the prose of each section, the
 * head's sentence naming the script that wrote the lists replaced by one
 * naming compass generate, and rewrites the link list under each heading from the recipes under
 * doc/recipes/shell, grouped by the category of their filetags or their
 * directory and sorted by title; a category with no section is appended,
 * a section no category claims is dropped. The HTTP inventory is rewritten
 * whole from its fixed frontmatter, which names compass generate as its
 * writer, and the recipes one directory under
 * doc/recipes/http, keeping the paragraph under each heading. Both keep the
 * note written beside a link. An inventory holding a merge conflict
 * marker, or a missing shell inventory, is not written.
 */
extern const compass::codec::types::descriptor recipe_inventories;

}

#endif
