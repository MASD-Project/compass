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
#ifndef COMPASS_INGEST_TESTS_DUMP_OF_HPP
#define COMPASS_INGEST_TESTS_DUMP_OF_HPP

#include <sstream>
#include <string>
#include <string_view>
#include "compass.ingest/org/parser.hpp"
#include "compass.org/io/dump.hpp"

namespace compass::ingest::tests {

/**
 * @brief Parses source and returns the dump of its tables.
 */
inline std::string dump_of(std::string_view source) {
    compass::org::types::tables tables;
    compass::ingest::org::parse(source, tables);
    std::ostringstream os;
    compass::org::io::dump(os, source, tables);
    return os.str();
}

}

#endif
