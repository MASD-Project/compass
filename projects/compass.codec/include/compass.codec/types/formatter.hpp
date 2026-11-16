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
#ifndef COMPASS_CODEC_TYPES_FORMATTER_HPP
#define COMPASS_CODEC_TYPES_FORMATTER_HPP

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace compass::codec::types {

/**
 * @brief The argument a formatter's arguments name its style file by. It is
 * replaced by the style file's path when the formatter runs, so the cache
 * key holds the style file's bytes, not where the checkout keeps it.
 */
inline constexpr std::string_view style_placeholder = "{style}";

/**
 * @brief An external program that rewrites output files in place, and the
 * outputs it applies to.
 *
 * A run is the program, then the arguments, then the files, run @p passes
 * times over the same files. The formatted form of a file is taken to
 * depend only on its bytes, the program's version, the arguments and the
 * bytes of the style file: the cache keys it by those and nothing else.
 */
struct formatter {
    std::string name;
    std::filesystem::path program;
    std::vector<std::string> extensions;
    std::vector<std::string> arguments;
    unsigned passes = 1;
    std::filesystem::path style;
};

}

#endif
