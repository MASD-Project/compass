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
#ifndef COMPASS_CODEC_OPS_EXCEPTIONS_HPP
#define COMPASS_CODEC_OPS_EXCEPTIONS_HPP

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/exception.hpp"
#include "compass.codec/types/registry.hpp"

namespace compass::codec::ops {

/**
 * @brief The text an exception comment starts with.
 */
inline constexpr std::string_view exception_marker = "compass:except";

/**
 * @brief The date @p s writes as YYYY-MM-DD, or nothing when it is not
 * of that form or no such day exists.
 */
[[nodiscard]] std::optional<std::chrono::year_month_day> parse_date(
    std::string_view s);

/**
 * @brief The exception comments of @p text, the bytes of the file at
 * @p path.
 *
 * An exception is written in a comment as
 * compass:except(<codec>/<rule>) expires=<YYYY-MM-DD> approver=<name>
 * reason=<text>, with compass:except-file( for one of file scope. The
 * reason runs to the end of the line, less the closer of a block comment
 * or of an HTML comment. The marker counts only right after a comment
 * opener: in an org file a # that starts the line, elsewhere a line
 * comment (//, --, #, ;) or a block or HTML comment opener. An exception
 * with code before its opener covers its own line; one alone on its line
 * covers the next line that is not itself such a comment, so exceptions
 * stack.
 */
[[nodiscard]] types::file_exceptions parse_exceptions(std::string_view path,
    std::string_view text);

/**
 * @brief Whether a rule named codec/rule is one no codec of a registry
 * declares, one of a codec that does not run, or one of a codec that
 * runs. A codec that emits files declares drift.
 */
enum class rule_status : std::uint8_t { unknown, disabled, enabled };

[[nodiscard]] rule_status status_of(const types::registry& codecs,
    std::string_view rule);

/**
 * @brief Where a finding is: the path of its file, its 1-based line and
 * its rule as codec/rule.
 */
struct finding_site {
    std::string_view path;
    std::uint32_t line = 0;
    std::string_view rule;
};

/**
 * @brief A finding about an exception, under the codec name exception.
 */
struct exception_finding {
    std::string path;
    std::uint32_t line = 0;
    std::string rule;
    std::string message;
    std::string fix;
};

/**
 * @brief What excuse decided: excused holds 1 for each site an exception
 * excuses, in the order of the sites, and findings the problems of the
 * exceptions, by path and line.
 */
struct excusal {
    std::vector<std::uint8_t> excused;
    std::vector<exception_finding> findings;
};

/**
 * @brief Excuses each of @p sites that a valid exception of @p files
 * covers, as of @p today.
 *
 * A valid exception names a rule an enabled codec of @p codecs declares,
 * gives a reason, an approver and an expiry, and has not expired: it is
 * valid up to and including its expiry date. It excuses the sites of its
 * rule in its file on the line it covers, or anywhere for file scope.
 * Every other exception excuses nothing and is a finding:
 * exception/malformed on its line for one that did not parse, names an
 * unknown rule or names an exception rule; exception/incomplete on its
 * line for one missing a field; exception/expired on the line it covers,
 * or its own for file scope, for one past its expiry; exception/stale on
 * its line for a valid one of an enabled codec that excuses no site. An
 * exception of a disabled codec is never stale.
 */
[[nodiscard]] excusal excuse(std::span<const finding_site> sites,
    std::span<const types::file_exceptions> files,
    const types::registry& codecs, std::chrono::year_month_day today);

}

#endif
