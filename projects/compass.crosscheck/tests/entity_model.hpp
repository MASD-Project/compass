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
#ifndef COMPASS_CROSSCHECK_TESTS_ENTITY_MODEL_HPP
#define COMPASS_CROSSCHECK_TESTS_ENTITY_MODEL_HPP

#include <string>
#include <string_view>

namespace compass::crosscheck::tests {

/**
 * @brief A column heading of a model's Columns section, with its drawer.
 */
inline std::string model_column(std::string_view name,
    std::string_view drawer) {
    return "** " + std::string(name) + "\n:PROPERTIES:\n" +
        std::string(drawer) + ":END:\n";
}

/**
 * @brief An ores.codegen entity model of component @p component that code
 * generation loads: the singular @p singular, the table @p table, the
 * file drawer lines @p drawer, the Flags drawer lines @p flags, the
 * Columns section's headings @p columns, the SQL flags @p sql_flags and
 * @p rest after them, under the SQL section.
 */
inline std::string entity_model(std::string_view component,
    std::string_view singular, std::string_view table,
    std::string_view columns, std::string_view rest = {},
    std::string_view flags = {}, std::string_view drawer = {},
    std::string_view sql_flags = {}) {
    return ":PROPERTIES:\n:ID: 0F4C1D7E-3D53-4E4A-9E0B-6E7B6C2D9A10\n" +
        std::string(drawer) + ":END:\n#+title: ores." +
        std::string(component) + "." + std::string(singular) +
        "\n#+type: ores.codegen.entity\n#+component: " +
        std::string(component) + "\n#+entity_singular: " +
        std::string(singular) + "\n#+entity_plural: " +
        std::string(singular) + "s\n\n* Flags\n:PROPERTIES:\n"
        ":schema: public\n:product: ores\n:component: " +
        std::string(component) + "\n:subcomponent: api\n" +
        std::string(flags) + ":END:\n\n* Columns\n\n" + std::string(columns) +
        "\n* SQL\n\n** Flags\n:PROPERTIES:\n:tablename: " +
        std::string(table) + "\n" + std::string(sql_flags) + ":END:\n\n" +
        std::string(rest);
}

}

#endif
