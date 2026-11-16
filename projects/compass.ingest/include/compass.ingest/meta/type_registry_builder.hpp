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
#ifndef COMPASS_INGEST_META_TYPE_REGISTRY_BUILDER_HPP
#define COMPASS_INGEST_META_TYPE_REGISTRY_BUILDER_HPP

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "compass.org/types/tables.hpp"
#include "compass.org/types/type_registry.hpp"

namespace compass::ingest::meta {

/**
 * @brief Where the level of a contract came from.
 *
 * unknown_value is a Level item whose first verbatim is not a level.
 */
enum class level_source : std::uint8_t { missing, item, unknown_value };

/**
 * @brief Where the folder of a contract came from: a Folder item or a Path
 * item.
 */
enum class folder_source : std::uint8_t { missing, folder_item, path_item };

/**
 * @brief Where the TODO vocabulary of a contract came from.
 *
 * todo_vocabulary_item is a "TODO vocabulary ::" item, state_item a
 * "State :: =#+todo: ...=" item, todo_block a #+todo: line in a block, and
 * not_stateful an item that opens with "Not stateful".
 */
enum class state_source : std::uint8_t {
    missing,
    todo_vocabulary_item,
    state_item,
    todo_block,
    not_stateful
};

/**
 * @brief Where the required sections of a contract came from: a
 * "Required ... ::" item or a sub-heading whose title names sections.
 */
enum class sections_source : std::uint8_t { missing, item, heading };

/**
 * @brief How one contract page fitted the extraction rule.
 *
 * types counts the names on the title. sections counts the entries of the
 * section list that name a heading.
 */
struct contract_report {
    std::uint8_t types = 0;
    bool contract = false;
    bool duplicate = false;
    level_source level = level_source::missing;
    folder_source folder = folder_source::missing;
    state_source state = state_source::missing;
    sections_source sections_from = sections_source::missing;
    std::uint16_t sections = 0;
};

/**
 * @brief True when every field of the contract was found.
 */
[[nodiscard]] bool fits(const contract_report& r) noexcept;

/**
 * @brief The #+type: values of ORE Studio's codegen models.
 *
 * They are the types ores.codegen's loader and manifest dispatch on. Their
 * schemas live in the codegen meta-model, not in doc/meta, so they enter the
 * registry by name only.
 */
inline constexpr std::array<std::string_view, 16> model_types{
    "ores.codegen.asset_class_catalogue",
    "ores.codegen.component",
    "ores.codegen.component_catalogue",
    "ores.codegen.dataset",
    "ores.codegen.domain_entity",
    "ores.codegen.entity",
    "ores.codegen.family",
    "ores.codegen.field_group",
    "ores.codegen.junction",
    "ores.codegen.lookup_entity",
    "ores.codegen.modeline_catalogue",
    "ores.codegen.module",
    "ores.codegen.operation",
    "ores.codegen.outcome_catalogue",
    "ores.codegen.service_registry",
    "ores.codegen.trade_type_catalogue"};

/**
 * @brief Collects types from contract pages and model names, then builds
 * the registry.
 *
 * A contract page is a document_type_*.org page parsed into @p page. Its
 * title "Document type: a, b" names the types it defines. The fields come
 * from the subtree of its level-1 "Contract" heading:
 *
 * - level: the first verbatim of a "Level ::" item;
 * - folder: the first verbatim of a "Folder ::" or "Path ::" item;
 * - TODO keywords: the verbatim of a "TODO vocabulary ::" item, of a
 *   "State :: =#+todo: ...=" item, or a #+todo: line in a block;
 * - required sections: the items of the list under a "Required ... ::" item,
 *   or under a sub-heading whose title names sections, that open with a
 *   verbatim heading such as =* Goal=.
 *
 * A name added twice keeps its first row and the second add reports a
 * duplicate.
 */
class type_registry_builder {
public:
    contract_report add_contract(std::string_view source,
        const compass::org::types::tables_view& page);

    /**
     * @brief Adds a model type; false when the name is already registered.
     */
    bool add_model(std::string_view name);

    [[nodiscard]] compass::org::types::type_registry build() const;

private:
    struct row {
        std::string name;
        compass::org::types::type_kind kind;
        compass::org::types::type_level level;
        std::string folder;
        std::vector<std::string> active;
        std::vector<std::string> done;
        std::vector<std::string> sections;
    };

    [[nodiscard]] bool has(std::string_view name) const;

    std::vector<row> rows_;
};

}

#endif
