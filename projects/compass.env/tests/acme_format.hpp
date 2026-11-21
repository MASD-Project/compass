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
#ifndef COMPASS_ENV_TESTS_ACME_FORMAT_HPP
#define COMPASS_ENV_TESTS_ACME_FORMAT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"

namespace compass::env::tests {

using row = std::vector<std::string>;

/**
 * @brief The tables of a made-up project, Acme, whose environment file
 * has a version, inputs, an offset port, a secret, kept values, a
 * per-service block, a conditional variable, carried credentials and a
 * removed variable.
 */
struct acme {
    std::vector<row> settings{
        {"file", ".env"},
        {"version_variable", "ACME_ENV_VERSION"},
        {"service_registry", "registry.org"},
        {"baseline", "1"}};
    std::vector<row> versions{
        {"1", "2026-01-01", "", "", "", "Initial."},
        {"2", "2026-02-01", "ACME_PORT", "ACME_OLD", "", "Ports."},
        {"3", "2026-03-01", "", "", "yes", "Moved ports."}};
    std::vector<row> variables{
        {"ACME_ENV_VERSION", "version", "", "1", "", "head", "", "", "",
            "The format version."},
        {"ACME_NAME", "value", "{name}", "1", "", "identity", "", "", "",
            "The environment."},
        {"ACME_PORT", "offset", "{base} 5", "2", "", "identity", "", "", "",
            "The port."},
        {"ACME_OLD", "", "", "1", "2", "", "", "", "", "Gone."},
        {"ACME_HOST", "kept", "localhost", "1", "", "identity", "", "", "",
            "The host."},
        {"ACME_URL", "value", "http://{ACME_HOST}:{ACME_PORT}", "1", "",
            "identity", "url", "", "", "The address."},
        {"ACME_DB_PASSWORD", "secret", "", "1", "", "secrets", "", "", "",
            "A password."},
        {"PGPORT", "kept", "", "1", "", "late", "", "{PGPORT}", "",
            "Written when set."},
        {"ACME_{service.key}_USER", "value", "acme_{name}_{service.key}",
            "1", "", "services", "gap", "", "", "A service's user."},
        {"ACME_{service.key}_POOL", "value", "16", "1", "", "services", "",
            "", "big", "A big pool."},
        {"ACME_{service.key}_POOL", "value", "8", "1", "", "services", "",
            "", "!big", "A pool."},
        {"ACME_{service.key}_PORT", "value", "{PGPORT:-5432}", "1", "",
            "services", "", "", "", "A service's database port."},
        {"ACME_SOURCE_KEY", "carried", "ACME_SOURCE_[A-Z]+_KEY", "1", "",
            "sources", "", "", "", "Source keys."}};
    std::vector<row> comments{
        {"head", "# Acme environment: {name}"},
        {"head", "# Generated on {now}"},
        {"head", ""},
        {"identity", ""},
        {"identity", "# Identity"},
        {"url", "# The address, {{scheme}}://host:port."},
        {"secrets", ""},
        {"secrets", "# Secrets"},
        {"services", ""},
        {"services", "# Services"},
        {"gap", ""},
        {"late", ""},
        {"late", "# Late"},
        {"sources", ""},
        {"sources", "# Sources"}};
    std::vector<row> inputs{
        {"name", "The environment's name."},
        {"base", "The base port."}};

    [[nodiscard]] codec::ops::data_tables tables() const;
};

inline codec::ops::data_tables acme::tables() const {
    codec::ops::data_tables out{.source = "compass/checks/env_format.org",
        .tables = {}};
    std::uint32_t line = 10;
    const auto add = [&](std::string name, std::vector<std::string> columns,
        const std::vector<row>& rows) {
        codec::ops::data_table t{.name = std::move(name), .line = line++,
            .columns = std::move(columns), .rows = {}, .row_lines = {}};
        for (const auto& r : rows) {
            t.rows.push_back(r);
            t.row_lines.push_back(line++);
        }
        out.tables.push_back(std::move(t));
    };
    add("Settings", {"setting", "value"}, settings);
    add("Versions", {"version", "date", "adds", "removes", "format only",
        "description"}, versions);
    add("Variables", {"variable", "kind", "argument", "added", "removed",
        "section", "comment", "when", "services", "purpose"}, variables);
    add("Comments", {"comment", "text"}, comments);
    add("Inputs", {"input", "purpose"}, inputs);
    return out;
}

/**
 * @brief Acme with a kept switch, ACME_POOLER, that picks between two rows
 * of ACME_DB and of the per-service ACME_{service.key}_DB, and writes a
 * per-service listener only when on.
 */
inline acme pooled_acme() {
    acme a;
    a.variables.insert(a.variables.begin() + 7, {
        {"ACME_POOLER", "kept", "1", "1", "", "secrets", "", "", "",
            "1 through the pooler, 0 direct."},
        {"ACME_DB", "value", "{ACME_PORT}", "1", "", "secrets", "",
            "{ACME_POOLER} != 0", "", "The pooler's port."},
        {"ACME_DB", "value", "5432", "1", "", "secrets", "",
            "{ACME_POOLER} = 0", "", "The database's port."},
        {"ACME_DB_URL", "value", "db:{ACME_DB}", "1", "", "secrets", "", "",
            "", "The database's address."}});
    a.variables.insert(a.variables.end() - 1, {
        {"ACME_{service.key}_DB", "value", "{ACME_PORT}", "1", "",
            "services", "", "{ACME_POOLER} != 0", "", "The pooler's port."},
        {"ACME_{service.key}_DB", "value", "5432", "1", "", "services", "",
            "{ACME_POOLER} = 0", "", "The database's port."},
        {"ACME_{service.key}_LISTENER", "value", "5432", "1", "",
            "services", "", "{ACME_POOLER} != 0", "", "A listener's port."}});
    return a;
}

/**
 * @brief Acme's service registry: two services with a key, and one
 * without, which per-service variables skip.
 */
inline constexpr std::string_view acme_registry =
    "#+title: Registry\n\n"
    "* small\n"
    ":PROPERTIES:\n"
    ":key: SMALL\n"
    ":replicas: 1\n"
    ":END:\n\n"
    "* big\n"
    ":PROPERTIES:\n"
    ":key: BIG\n"
    ":replicas: 2\n"
    ":END:\n\n"
    "* proxy\n"
    ":PROPERTIES:\n"
    ":replicas: 1\n"
    ":END:\n";

}

#endif
