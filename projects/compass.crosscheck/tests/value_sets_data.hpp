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
#ifndef COMPASS_CROSSCHECK_TESTS_VALUE_SETS_DATA_HPP
#define COMPASS_CROSSCHECK_TESTS_VALUE_SETS_DATA_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "test_checkout.hpp"

namespace compass::crosscheck::tests {

inline constexpr std::string_view value_sets_document =
    "compass/checks/value_sets.org";

/**
 * @brief Each role of the value set entities, and the singular and plural
 * of its entity without a prefix.
 */
inline constexpr std::array<std::array<std::string_view, 3>, 6>
    value_set_roles{{
        {"vocabulary", "parameter_vocabulary", "parameter_vocabularies"},
        {"element_map_column", "element_map_column", "element_map_columns"},
        {"value_set", "parameter_value_set", "parameter_value_sets"},
        {"value_set_value", "parameter_value_set_value",
         "parameter_value_set_values"},
        {"value_set_catalogue", "parameter_value_set_catalogue",
         "parameter_value_set_catalogues"},
        {"setting_value_set", "parameter_setting_value_set",
         "parameter_setting_value_sets"}}};

/**
 * @brief The rows of a Value set entities table: each role, and its
 * entity's singular after @p prefix.
 */
inline std::vector<std::vector<std::string>> value_set_entities(
    std::string_view prefix) {
    std::vector<std::vector<std::string>> out;
    for (const auto& [role, singular, plural] : value_set_roles)
        out.push_back({std::string(role), std::string(prefix) +
            std::string(singular)});
    return out;
}

/**
 * @brief The models of the value set entities, as ORE Studio states them:
 * under projects/<product>.<component>/modeling/, each entity's singular
 * after @p prefix, its table <product>_<component>_<plural>_tbl in the SQL
 * section's flags.
 */
inline files value_set_models(std::string_view component = "reporting",
    std::string_view prefix = "ore_", std::string_view product = "ores") {
    files out;
    const auto c = std::string(component);
    const auto p = std::string(product);
    for (const auto& [role, bare, bare_plural] : value_set_roles) {
        const auto singular = std::string(prefix) + std::string(bare);
        const auto plural = std::string(prefix) + std::string(bare_plural);
        out["projects/" + p + "." + c + "/modeling/" + p + "." + c + "." +
            singular + ".org"] = "#+title: " + p + "." + c + "." +
            singular + "\n#+type: ores.codegen.entity\n#+component: " + c +
            "\n#+entity_singular: " + singular + "\n#+entity_plural: " +
            plural + "\n\n* Flags\n:PROPERTIES:\n:schema: public\n"
            ":product: " + p + "\n:component: " + c + "\n:END:\n\n"
            "* SQL\n\n** Flags\n:PROPERTIES:\n:tablename: " + p + "_" + c +
            "_" + plural + "_tbl\n:END:\n";
    }
    return out;
}

/**
 * @brief The tables of a value sets check document, as a test builds
 * them: every table the codecs read, with its columns and no rows, but
 * for the three that state one value, which hold a row, and Value set
 * entities, which names ORE Studio's entities (value_set_models()).
 */
struct value_sets_data {
    std::vector<compass::codec::ops::data_table> tables{
        {.name = "Catalogues",
         .columns = {"code", "name", "relation", "description"}},
        {.name = "Catalogue kinds", .columns = {"kind", "catalogue"}},
        {.name = "Rendered kinds", .columns = {"kind", "renderings"}},
        {.name = "Kinds without a list", .columns = {"kind", "set"}},
        {.name = "Kinds one parser lists", .columns = {"kind", "parser"}},
        {.name = "Type sets", .columns = {"family", "type", "set"}},
        {.name = "Schema type sets", .columns = {"kind", "type"}},
        {.name = "Convention own sets", .columns = {"code", "name",
            "description", "source_kind", "pattern", "is_ordered"}},
        {.name = "Type overrides",
         .columns = {"element", "term", "kind", "reason"}},
        {.name = "Curation keys", .columns = {"key"}},
        {.name = "Curation sources", .columns = {"source"}},
        {.name = "List items",
         .columns = {"leaf", "kind", "source", "source_ref", "reason"}},
        {.name = "Period pattern", .columns = {"pattern"},
         .rows = {{"^[0-9]+[DWMY]$"}}},
        {.name = "Parser tables", .columns = {"parser", "table"}},
        {.name = "Rendering families",
         .columns = {"table", "name", "description", "label"}},
        {.name = "Schema kinds", .columns = {"xsd_type", "kind"}},
        {.name = "Unread leaves allowed", .columns = {"count"},
         .rows = {{"0"}}},
        {.name = "Item words", .columns = {"kind", "words"}},
        {.name = "Schema words", .columns = {"kind", "name", "words"}},
        {.name = "Asset classes", .columns = {"asset_class"}},
        {.name = "Not a column",
         .columns = {"kind", "variable", "column", "reason"}},
        {.name = "Convention schema", .columns = {"schema"},
         .rows = {{"conventions.xsd"}}},
        {.name = "File kinds", .columns = {"file", "kind"}},
        {.name = "Spelled otherwise", .columns = {"term", "xpath", "reason"}},
        {.name = "Read elsewhere", .columns = {"kind", "element", "reason"}},
        {.name = "Kind elements", .columns = {"kind", "element"}},
        {.name = "Member entities", .columns = {"member", "entity"}},
        {.name = "Value set entities", .columns = {"role", "entity"},
         .rows = value_set_entities("ore_")}};

    /**
     * @brief The rows of the table @p name.
     */
    std::vector<std::vector<std::string>>& operator[](std::string_view name) {
        const auto it = std::ranges::find(tables, name,
            &compass::codec::ops::data_table::name);
        if (it == tables.end())
            throw std::logic_error("no table " + std::string(name));
        return it->rows;
    }

    /**
     * @brief The data option compass sets from a document holding these
     * tables, each table and row on a line of its own.
     */
    [[nodiscard]] options settings() const {
        namespace cops = compass::codec::ops;
        cops::data_tables d{.source = std::string(value_sets_document)};
        std::uint32_t line = 1;
        for (auto t : tables) {
            t.line = line++;
            t.row_lines.clear();
            for (std::size_t r = 0; r < t.rows.size(); ++r)
                t.row_lines.push_back(line++);
            d.tables.push_back(std::move(t));
        }
        return {{std::string(cops::data_option), cops::encode_data_tables(d)}};
    }

    /**
     * @brief settings() without the table @p name.
     */
    [[nodiscard]] options without(std::string_view name) const {
        auto copy = *this;
        std::erase_if(copy.tables, [&](const auto& t) { return t.name == name; });
        return copy.settings();
    }
};

}

#endif
