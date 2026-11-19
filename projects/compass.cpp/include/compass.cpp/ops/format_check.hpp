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
#ifndef COMPASS_CPP_OPS_FORMAT_CHECK_HPP
#define COMPASS_CPP_OPS_FORMAT_CHECK_HPP

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/formatter.hpp"

/**
 * @brief The format engine: a check that every file it covers is as a
 * formatter formats it.
 */
namespace compass::cpp::ops {

/**
 * @brief A format check: the files it covers and the formatter they must
 * already be formatted by.
 *
 * files and outside are globs as add_globs reads them. formatter is one of
 * the formatters compass knows, by name. programs are the candidates for
 * its program, in turn: a name on PATH, or a path relative to the
 * checkout. style is the formatter's configuration file, relative to the
 * checkout.
 */
struct format_definition {
    std::string source;
    std::vector<std::string> files;
    std::vector<std::string> outside;
    std::string formatter;
    std::vector<std::string> programs;
    std::string style;
};

/**
 * @brief The definition @p text states, or the first error in it as
 * "line <n>: <what>".
 *
 * The text is one setting per line, a key, a space and its value, as a
 * forbid definition's is. SOURCE names the file the check is written in.
 * FILES and OUTSIDE take globs; FORMATTER takes prettier or clang-format;
 * PROGRAM takes the space-separated candidates for its program; STYLE
 * takes the path of its configuration file. FILES, FORMATTER, PROGRAM and
 * STYLE are required. A key given twice, an unknown key, RULE and EXAMPLE
 * are errors: the check has one rule, unformatted, and no examples.
 */
[[nodiscard]] std::expected<format_definition, std::string>
parse_format_definition(std::string_view text);

/**
 * @brief The formatter @p d names, with its program, the first of its
 * candidates found for the checkout @p checkout, and its style file under
 * the checkout.
 * @throws std::runtime_error when none of the candidates is a program.
 */
[[nodiscard]] compass::codec::types::formatter formatter_of(
    const format_definition& d, const std::filesystem::path& checkout);

}

#endif
