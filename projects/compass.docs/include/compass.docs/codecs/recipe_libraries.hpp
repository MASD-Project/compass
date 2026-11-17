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
#ifndef COMPASS_DOCS_CODECS_RECIPE_LIBRARIES_HPP
#define COMPASS_DOCS_CODECS_RECIPE_LIBRARIES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's tangle-shell generator
 * (ores-build-recipe-scripts.el): the ores-shell script library tangled
 * from the shell recipes.
 *
 * A document codec over the recipes under the source option. A recipe
 * whose ores-shell blocks name :tangle targets gives one script per target,
 * named by the target's file name; any other recipe with an ores-shell
 * block gives one script named after itself. Scripts go to the library
 * option's directory, in the folder the recipe's :recipe:shell:<category>:
 * filetag names, or general. A trailing exit line is dropped and a banner
 * of # comment lines leads: the section heading, or the title and the
 * description, then the GENERATED line naming the recipe.
 */
extern const compass::codec::types::descriptor shell_recipe_library;

/**
 * @brief ORE Studio's tangle-http generator (ores-build-http-recipes.el):
 * the Hurl library tangled from the HTTP recipes.
 *
 * As shell_recipe_library for hurl blocks, with :recipe:http:<category>:
 * filetags, but every recipe names its targets, nothing is stripped, and
 * the banner never carries the description.
 */
extern const compass::codec::types::descriptor http_recipe_library;

}

#endif
