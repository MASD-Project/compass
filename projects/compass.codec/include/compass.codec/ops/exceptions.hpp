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

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/descriptor.hpp"
#include "compass.codec/types/exception.hpp"
#include "compass.codec/types/registry.hpp"

namespace compass::codec::ops {

/**
 * @brief The text an exception comment starts with.
 */
inline constexpr std::string_view exception_marker = "compass:except";

/**
 * @brief The codec name the findings about exceptions carry.
 */
inline constexpr std::string_view exception_codec = "exception";

inline constexpr std::array<std::string_view, 1> exception_documents{
    "5B528519-8BCE-4CB8-A48E-B525FD71F873"};

/**
 * @brief The rules of the findings about exceptions, which the runtime
 * reports under exception_codec rather than a codec declaring them.
 */
enum class exception_rule : std::uint8_t {
    malformed, incomplete, expired, stale };

inline constexpr std::array<types::rule, 4> exception_rules{{
    {"malformed", "An exception comment parses and names a rule some codec "
        "declares, other than a rule about exceptions.", exception_documents},
    {"incomplete", "An exception gives a reason, an approver and an "
        "expiry.", exception_documents},
    {"expired", "No exception is used past its expiry date.",
        exception_documents},
    {"stale", "A valid exception of an enabled codec excuses some "
        "finding.", exception_documents}}};

[[nodiscard]] constexpr const types::rule& rule_of(exception_rule r) noexcept {
    return exception_rules[static_cast<std::size_t>(r)];
}

inline constexpr std::array<std::string_view, 2> drift_documents{
    "B71DDE37-CFC8-44C1-A3DA-4B50C4CBE3EE",
    "C88C4034-1DE6-4945-8CF1-393EC9E0805F"};

/**
 * @brief The rule every codec that emits files declares without listing
 * it: the file on disk holds what the codec generates.
 */
inline constexpr types::rule drift_rule{"drift", "Every file a codec "
    "generates holds what the codec generates from its inputs; nobody edits "
    "it by hand.", drift_documents};

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
