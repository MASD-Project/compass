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
#ifndef COMPASS_STORE_OPS_GIT_SETTINGS_HPP
#define COMPASS_STORE_OPS_GIT_SETTINGS_HPP

#include <string_view>
#include "compass.store/types/git_settings.hpp"

namespace compass::store::ops {

/**
 * @brief Folds one git configuration file into @p config.
 *
 * A minimal reader of the syntax of git-config(1): sections with or without
 * a subsection, case-insensitive names, comments, quoted values and line
 * continuations. It looks at core.autocrlf, core.attributesFile,
 * extensions.objectFormat, include.path and includeIf.*.path, and ignores
 * everything else. Conditions of includeIf are not evaluated: every
 * conditional include is read, which can only add distrust.
 */
void read_git_config(std::string_view text, types::git_config& config);

/**
 * @brief Adds the rules of one attributes file whose directory, relative to
 * the checkout, is @p base: empty for the root, else ending in '/'.
 *
 * Only rules that can convert a file are kept; see attribute_rules. A
 * pattern git would not apply to a file (a negative pattern, or one ending
 * in '/') is dropped. A pattern with a character class such as [:alpha:]
 * is kept as one that matches every path under @p base, and an unclosed
 * bracket matches whatever text is left.
 */
void read_attributes(std::string_view text, std::string_view base,
    types::attribute_rules& rules);

/**
 * @brief True when any rule matches @p path, a path relative to the
 * checkout, with the pattern rules of gitignore(5): a pattern with no slash
 * matches the file name at any depth below its base, and any other pattern
 * matches the path relative to its base, with '*' and '?' stopping at '/'
 * and '**' crossing directories.
 */
[[nodiscard]] bool converts(const types::attribute_rules& rules,
    std::string_view path);

}

#endif
