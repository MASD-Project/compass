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
#ifndef COMPASS_BENCH_OPS_ORG_DUMP_HPP
#define COMPASS_BENCH_OPS_ORG_DUMP_HPP

#include <cstddef>
#include <filesystem>
#include <ostream>

namespace compass::bench::ops {

/**
 * @brief Parses one org file and writes the dump of its tables.
 *
 * The format is the one of compass::org::io::dump, which
 * build/scripts/org_element_dump.el prints for org-element.
 */
void dump_org_file(const std::filesystem::path& file, std::ostream& os);

/**
 * @brief Dumps every file a list names and returns how many it wrote.
 *
 * Each line of @p list is an input path, a tab and an output path. A dump
 * that cannot be read or written throws.
 */
std::size_t dump_org_list(const std::filesystem::path& list);

}

#endif
