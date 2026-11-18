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
#ifndef COMPASS_CODEC_TYPES_EXCEPTION_HPP
#define COMPASS_CODEC_TYPES_EXCEPTION_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace compass::codec::types {

/**
 * @brief What an exception excuses: the findings of its rule on the line
 * it covers, or every finding of its rule in its file.
 */
enum class exception_scope : std::uint8_t { line, file };

/**
 * @brief One exception that parsed, from a comment or from a row of the
 * exceptions table of the configuration file.
 *
 * written_in is the path of the file the exception is written in and
 * line its 1-based line there, where the findings about it go. path is
 * the file whose findings it excuses, the same as written_in for a
 * comment. covers is the 1-based line of path it excuses findings on, or
 * 0 for an exception of file scope. rule is the codec and rule name as
 * codec/rule. reason and approver are empty and expires none when the
 * exception does not give them; such an exception is incomplete and
 * excuses nothing. Paths are relative to the root of the checkout.
 */
struct exception {
    std::string written_in;
    std::uint32_t line = 0;
    std::string path;
    std::uint32_t covers = 0;
    exception_scope scope = exception_scope::line;
    std::string rule;
    std::string reason;
    std::string approver;
    std::optional<std::chrono::year_month_day> expires;
};

/**
 * @brief An exception that did not parse: the file it is written in, its
 * 1-based line there and why.
 */
struct malformed_exception {
    std::string written_in;
    std::uint32_t line = 0;
    std::string why;
};

/**
 * @brief Exceptions from any number of sources, comments and table rows
 * alike.
 */
struct exception_set {
    std::vector<exception> exceptions;
    std::vector<malformed_exception> malformed;
};

}

#endif
