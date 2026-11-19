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
#include "compass.cpp/codecs/format_check.hpp"

#include <stdexcept>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/format_check.hpp"
#include "compass.cpp/ops/glob.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;

void run(const ct::corpus_view& v, ct::file_output& out) {
    const auto d = ops::parse_format_definition(
        v.option(format_definition_key));
    if (!d)
        return;
    for (const auto path : ops::covered_files(v, d->files, d->outside)) {
        if (const auto bytes = v.read_file(path))
            out.files.add(path, *bytes);
    }
}

std::vector<ct::formatter> formatters(const ct::option_values& o,
    const std::filesystem::path& checkout) {
    const auto d = ops::parse_format_definition(o[format_definition_key]);
    if (!d)
        throw std::runtime_error("a format check's definition does not "
            "parse: " + d.error());
    return {ops::formatter_of(*d, checkout)};
}

}

ct::corpus_function<ct::file_output> format_check_function() noexcept {
    return run;
}

ct::formatters_function format_check_formatters() noexcept {
    return formatters;
}

}
