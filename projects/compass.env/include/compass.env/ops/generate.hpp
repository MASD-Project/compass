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
#ifndef COMPASS_ENV_OPS_GENERATE_HPP
#define COMPASS_ENV_OPS_GENERATE_HPP

#include <cstddef>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "compass.env/types/env_format.hpp"

namespace compass::env::ops {

using values = std::map<std::string, std::string, std::less<>>;

/**
 * @brief The variables of an environment file: each name with the value of
 * its first line, as a shell that sources the file would not see them but
 * as generation keeps them. Blank lines, comments and lines without '='
 * are skipped.
 */
[[nodiscard]] values read_env(std::string_view text);

/**
 * @brief The services of a service registry: each top-level heading with
 * a :replicas: property, as the model loads one, in the order of the
 * document. Its fields are its drawer's properties and name, its heading.
 */
[[nodiscard]] std::vector<types::service> read_services(
    std::string_view source);

/**
 * @brief Whether the per-service variable @p v is written for @p s: @p s
 * has every field its name uses, and its services list names @p s, or
 * excludes it not.
 */
[[nodiscard]] bool applies_to(const types::variable& v,
    const types::service& s);

/**
 * @brief Everything generation reads besides the format.
 *
 * inputs holds the declared inputs and the built-in ones; overrides sets
 * variables outright, whatever their kind; existing is the file being
 * replaced; environment reads the process environment; read_file reads a
 * file a file variable names, relative paths against the checkout; secret
 * makes a secret of the given length.
 */
struct sources {
    values inputs;
    values overrides;
    values existing;
    std::function<std::optional<std::string>(const std::string&)>
        environment;
    std::function<std::optional<std::string>(const std::string&)> read_file;
    std::function<std::string(std::size_t)> secret;
};

/**
 * @brief A generated file: its text, and its variables in order.
 */
struct generated {
    std::string text;
    std::vector<std::string> names;
};

/**
 * @brief The environment file @p f describes, for @p services, from @p s.
 *
 * Rows are written in table order, each removed row skipped. A row's
 * section, a block of the Comments table, is written before it when the
 * previous written row had another; its comment before it every time it is
 * written. A row whose when expands to nothing is not written, nor are its
 * comments. Consecutive per-service rows are written service by service,
 * in the registry's order.
 *
 * @return the file, or why it cannot be made: an input missing or not
 * declared, an override of no variable, a template that does not expand,
 * an offset that is not a number, a file that cannot be read.
 */
[[nodiscard]] std::expected<generated, std::string> generate(
    const types::env_format& f, std::span<const types::service> services,
    const sources& s);

/**
 * @brief How an environment file stands against its format.
 */
struct status_report {
    std::optional<std::uint32_t> version;
    std::uint32_t current = 0;
    std::vector<std::string> missing;
    std::vector<std::string> removed;
    std::vector<std::string> undeclared;

    /**
     * @brief Whether the file must be generated again.
     */
    [[nodiscard]] bool stale() const {
        return !version || *version < current || !missing.empty() ||
            !removed.empty() || !undeclared.empty();
    }
};

/**
 * @brief How @p existing stands against @p f for @p services: its version,
 * the variables every file has that it lacks (those of rows without a
 * when), the removed variables it still holds, and those no row declares.
 */
[[nodiscard]] status_report status_of(const types::env_format& f,
    std::span<const types::service> services, const values& existing);

}

#endif
