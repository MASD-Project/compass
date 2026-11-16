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
#include "compass.bench/ops/org_dump.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include "compass.ingest/org/parser.hpp"
#include "compass.org/io/dump.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"

namespace compass::bench::ops {

void dump_org_file(const std::filesystem::path& file, std::ostream& os) {
    const platform::filesystem::mapped_file mapped(file);
    compass::org::types::tables tables;
    ingest::org::parse(mapped.text(), tables);
    compass::org::io::dump(os, mapped.text(),
        compass::org::types::view(tables));
}

std::size_t dump_org_list(const std::filesystem::path& list) {
    std::ifstream lines(list);
    if (!lines)
        throw std::runtime_error("cannot read " + list.string());
    std::size_t written = 0;
    std::string line;
    while (std::getline(lines, line)) {
        const auto tab = line.find('\t');
        if (tab == std::string::npos)
            throw std::runtime_error("no tab in list line: " + line);
        const auto output = line.substr(tab + 1);
        std::ofstream os(output, std::ios::binary);
        dump_org_file(line.substr(0, tab), os);
        os.flush();
        if (!os)
            throw std::runtime_error("cannot write " + output);
        ++written;
    }
    return written;
}

}
