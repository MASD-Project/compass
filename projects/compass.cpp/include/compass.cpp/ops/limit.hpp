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
#ifndef COMPASS_CPP_OPS_LIMIT_HPP
#define COMPASS_CPP_OPS_LIMIT_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/descriptor.hpp"
#include "compass.codec/types/diagnostic_table.hpp"

/**
 * @brief The limit engine: a check that no type of the files it covers
 * counts more of a fact than its ceiling, save the types it records, each
 * with a ceiling of its own that only comes down.
 */
namespace compass::cpp::ops {

/**
 * @brief The rules of a limit check, in the order of its findings' rule
 * indices.
 */
inline constexpr std::array<std::string_view, 2> limit_rule_names{
    "over_ceiling", "below_record"};

/**
 * @brief A type a limit check allows above its ceiling: the file that
 * defines it, its name and the count it may reach.
 */
struct recorded_ceiling {
    std::string path;
    std::string type;
    std::uint32_t ceiling = 0;
};

/**
 * @brief A limit check: the files it covers, the fact it counts, the
 * ceiling every type keeps to and the types recorded above it. The one
 * fact is cpp_fields, the non-static data members of each named class,
 * struct and union, as type_widths counts them.
 */
struct limit_definition {
    std::string source;
    std::vector<std::string> files;
    std::vector<std::string> outside;
    std::string fact;
    std::uint32_t ceiling = 0;
    std::vector<recorded_ceiling> recorded;
};

/**
 * @brief The definition @p text states, or the first error in it as
 * "line <n>: <what>".
 *
 * The text is one setting per line, a key, a space and its value, as a
 * forbid definition's is. SOURCE names the file the check is written in.
 * FILES and OUTSIDE take globs; FACT takes cpp_fields; CEILING takes a
 * count. Each RECORDED line takes a path, a type name and a count above
 * the ceiling, separated by spaces; a type recorded twice is an error.
 * FILES, FACT and CEILING are required. Any other key given twice, an
 * unknown key, RULE and EXAMPLE are errors: the check's rules are
 * limit_rule_names, and it has no examples.
 */
[[nodiscard]] std::expected<limit_definition, std::string>
parse_limit_definition(std::string_view text);

/**
 * @brief What a limit check keeps of the file whose bytes are @p bytes:
 * one line per type over the ceiling of @p d, its name, the offset of its
 * key, the key and the count, separated by tabs.
 */
[[nodiscard]] std::string limit_fact(const limit_definition& d,
    std::string_view bytes);

/**
 * @brief Reports, to @p out, each type of @p files, whose facts are
 * limit_fact's, over its ceiling: the recorded one, or the check's, as
 * over_ceiling at the type's key; and each recorded type now below its
 * record, or at or under the check's ceiling, or no longer in a covered
 * file, as below_record on the row of d.source, whose bytes are
 * @p source, that records it.
 */
void limit_findings(const limit_definition& d,
    std::span<const compass::codec::types::scanned_file> files,
    std::string_view source, compass::codec::types::diagnostic_table& out);

}

#endif
