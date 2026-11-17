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
#include "compass.docs/codecs/catalogue.hpp"

#include <array>
#include "compass.docs/codecs/component_docs.hpp"
#include "compass.docs/codecs/heading_numbers.hpp"
#include "compass.docs/codecs/org_links.hpp"
#include "compass.docs/codecs/pattern_uses.hpp"
#include "compass.docs/codecs/prototypes.hpp"
#include "compass.docs/codecs/recipe_libraries.hpp"
#include "compass.docs/codecs/rule_identifiers.hpp"
#include "compass.docs/codecs/tree_gates.hpp"

namespace compass::docs::codecs {

std::span<const compass::codec::types::descriptor> catalogue() noexcept {
    static const std::array<compass::codec::types::descriptor, 11> all{
        component_docs, heading_numbers, http_recipe_library, no_pr_triggers,
        org_links, pattern_uses, prototypes, rule_identifiers,
        shell_recipe_library, shell_recipe_provenance, skill_namespace};
    return all;
}

}
