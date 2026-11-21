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
#ifndef COMPASS_ENV_TYPES_ENV_FORMAT_HPP
#define COMPASS_ENV_TYPES_ENV_FORMAT_HPP

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace compass::env::types {

/**
 * @brief How the value of a variable is made.
 *
 * - value: its argument, a template.
 * - offset: the sum of the integers its argument's terms expand to.
 * - secret: the existing file's value, else a new random secret of the
 *   length its argument gives (32 when empty).
 * - kept: the existing file's value, else its argument, a template.
 * - inherited: the existing file's value, else the process environment's,
 *   else its argument.
 * - environment: the process environment's value, else its argument; never
 *   the existing file's.
 * - version: the highest version of the format.
 * - file: the lines of the file its argument names, each followed by the
 *   two characters \n, in double quotes.
 * - carried: every variable of the existing file whose name matches its
 *   argument, a regular expression, with its value, in name order.
 */
enum class value_kind : std::uint8_t {
    value, offset, secret, kept, inherited, environment, version, file,
    carried
};

/**
 * @brief One row of the Variables table.
 *
 * A name holding {service.<field>} is a per-service variable, written once
 * for each service of the registry that has every field it names. added
 * and removed are versions; 0 when the cell is empty.
 */
struct variable {
    std::string name;
    value_kind kind = value_kind::value;
    std::string argument;
    std::uint32_t added = 0;
    std::uint32_t removed = 0;
    std::string section;
    std::string comment;
    std::string when;
    std::vector<std::string> services;
    bool excludes_services = false;
    std::string purpose;
    std::uint32_t line = 0;

    [[nodiscard]] bool per_service() const;
};

/**
 * @brief One row of the Versions table: the variables it adds and removes,
 * or format_only when it changes only values, layout or the services the
 * registry expands.
 */
struct version {
    std::uint32_t number = 0;
    std::string date;
    std::vector<std::string> adds;
    std::vector<std::string> removes;
    bool format_only = false;
    std::string description;
    std::uint32_t line = 0;
};

/**
 * @brief An input the project's own set-up passes to generation.
 */
struct input {
    std::string name;
    std::string purpose;
};

/**
 * @brief A project's environment file, as its check document states it.
 */
struct env_format {
    std::string source;
    std::string file = ".env";
    std::string version_variable;
    std::string service_registry;
    std::string regenerate;
    std::uint32_t baseline = 0;
    std::vector<version> versions;
    std::vector<variable> variables;
    std::map<std::string, std::vector<std::string>, std::less<>> comments;
    std::vector<input> inputs;

    /**
     * @brief The highest version, or 0 when there is none.
     */
    [[nodiscard]] std::uint32_t current() const;
};

/**
 * @brief A service of the registry: its heading as name, and the
 * properties of its drawer.
 */
struct service {
    std::map<std::string, std::string, std::less<>> fields;
};

/**
 * @brief Something wrong with a format, at a line of its document; line 0
 * is the document.
 */
struct finding {
    std::uint32_t rule = 0;
    std::uint32_t line = 0;
    std::string message;
    std::string fix;
};

}

#endif
