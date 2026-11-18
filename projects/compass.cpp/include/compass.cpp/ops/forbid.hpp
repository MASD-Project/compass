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
#ifndef COMPASS_CPP_OPS_FORBID_HPP
#define COMPASS_CPP_OPS_FORBID_HPP

#include <expected>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/diagnostic_table.hpp"

/**
 * @brief The forbid engine: a check that reports each use of a C++ name,
 * #include or pattern its definition lists, in the files it covers.
 */
namespace compass::cpp::ops {

/**
 * @brief One rule of a forbid check: what it forbids and what its finding
 * says.
 *
 * cpp_names are names as written, components joined by ::. A name the
 * code writes matches when its leading components are those of one of
 * them, a leading :: aside, so std::jthread matches std::jthread::id and
 * getenv does not match ores::platform::getenv. A member access, as in
 * o.getenv, is no match. cpp_includes are #include targets with their
 * brackets or quotes, as <unistd.h>. cpp_text is a regular expression
 * over the code with its comments and literals blanked. text is a regular
 * expression over the file's bytes. message and fix may hold {match},
 * which becomes the listed name or include, or the text the expression
 * matched.
 */
struct forbid_rule {
    std::string name;
    std::vector<std::string> cpp_names;
    std::vector<std::string> cpp_includes;
    std::string cpp_text;
    std::string text;
    std::string message;
    std::string fix;
};

/**
 * @brief A forbid check: the files it covers and its rules. files and
 * outside are globs as glob_matches reads them, with each {a,b} already
 * expanded; a file is covered when it matches one of files and none of
 * outside.
 */
struct forbid_definition {
    std::vector<std::string> files;
    std::vector<std::string> outside;
    std::vector<forbid_rule> rules;
};

/**
 * @brief The definition @p text states, or the first error in it as
 * "line <n>: <what>".
 *
 * The text is one setting per line, a key, a space and its value; blank
 * lines are skipped. FILES and OUTSIDE take space-separated globs, in
 * which {a,b,c} stands for each of its alternatives and a last ** for
 * every file under its directory. RULE <name> starts a
 * rule; the keys after it are that rule's. CPP_NAME and CPP_INCLUDE take
 * space-separated values; CPP_TEXT and TEXT take one regular expression,
 * spaces and all; MESSAGE and FIX take their text. FILES and at least one
 * rule are required; each rule needs a name unique in the definition, at
 * least one of the four match keys, and a MESSAGE. A key given twice, an
 * unknown key, a nested or unclosed brace and an expression that does not
 * compile are errors.
 */
[[nodiscard]] std::expected<forbid_definition, std::string>
parse_forbid_definition(std::string_view text);

/**
 * @brief Whether @p d covers the checkout file @p path.
 */
[[nodiscard]] bool covers(const forbid_definition& d, std::string_view path);

/**
 * @brief The files of the checkout @p d covers, each once, in byte order.
 */
[[nodiscard]] std::vector<std::string_view> covered_files(
    const compass::codec::types::corpus_view& v, const forbid_definition& d);

/**
 * @brief Reports, to @p out, each line of the file @p path, whose bytes
 * are @p bytes, on which a rule of @p d matches: one finding per rule and
 * line, of the rule's index in @p d, at the start of the line. Code an
 * #if 0 excludes is not read by the C++ keys.
 */
void forbid_scan(const forbid_definition& d, std::string_view path,
    std::string_view bytes, compass::codec::types::diagnostic_table& out);

}

#endif
