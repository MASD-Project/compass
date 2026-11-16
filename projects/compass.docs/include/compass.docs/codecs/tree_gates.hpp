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
#ifndef COMPASS_DOCS_CODECS_TREE_GATES_HPP
#define COMPASS_DOCS_CODECS_TREE_GATES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's shell-recipe-provenance gate
 * (check_shell_recipe_sources.py): every shell recipe names the document
 * it was tangled from.
 *
 * Reads each .ores one directory under
 * projects/ores.shell/scripts/library/. Rule orphan_recipe: no line is
 * "# GENERATED from <path> - do not edit by hand." (with an em dash), or
 * the path names no file of the checkout.
 */
extern const compass::codec::types::descriptor shell_recipe_provenance;

/**
 * @brief ORE Studio's skill-namespace gate
 * (rename_skills_to_compass_namespace.py --check): every skill is named
 * compass-...
 *
 * A skill is a directory directly under doc/llm/skills that holds a
 * SKILL.org. Rule outside_namespace: its name does not start with
 * compass-.
 */
extern const compass::codec::types::descriptor skill_namespace;

/**
 * @brief ORE Studio's no-pr-triggers gate (check_no_pr_triggers.py): no
 * workflow triggers on a pull request.
 *
 * Reads each .yml and .yaml directly under .github/workflows. Rule
 * pull_request_trigger: a line that is exactly "pull_request:" indented by
 * two spaces, trailing spaces aside, and not a comment; pull_request_review
 * is another event. Rule no_workflows: there is no workflow file.
 */
extern const compass::codec::types::descriptor no_pr_triggers;

}

#endif
