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
#ifndef COMPASS_STORE_TYPES_GIT_SETTINGS_HPP
#define COMPASS_STORE_TYPES_GIT_SETTINGS_HPP

#include <string>
#include <vector>

namespace compass::store::types {

/**
 * @brief What git's configuration says about whether index IDs name the
 * bytes of the working tree, gathered from every file read.
 *
 * The facts only ever move towards distrust: autocrlf is true when any file
 * sets core.autocrlf to anything but false, and sha1 is false when any file
 * names an object format other than SHA-1. includes and attributes_files
 * hold the paths named by include.path, includeIf.*.path and
 * core.attributesFile, as written.
 */
struct git_config {
    bool autocrlf = false;
    bool sha1 = true;
    std::vector<std::string> includes;
    std::vector<std::string> attributes_files;
};

/**
 * @brief The .gitattributes rules that can make git convert a file between
 * the working tree and the object it hashes, as parallel columns.
 *
 * A rule is kept when it sets text, eol, crlf, filter, ident or
 * working-tree-encoding, or a macro that does. base is the directory of the
 * file that holds the rule, relative to the checkout and ending in '/', or
 * empty for the root. converting_macros names the macros found so far that
 * set one of those attributes.
 */
struct attribute_rules {
    std::vector<std::string> bases;
    std::vector<std::string> patterns;
    std::vector<std::string> converting_macros;
};

}

#endif
