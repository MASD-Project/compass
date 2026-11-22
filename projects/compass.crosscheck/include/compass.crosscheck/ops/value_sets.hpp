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
#ifndef COMPASS_CROSSCHECK_OPS_VALUE_SETS_HPP
#define COMPASS_CROSSCHECK_OPS_VALUE_SETS_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.crosscheck/ops/json_value.hpp"
#include "compass.crosscheck/ops/projection_columns.hpp"

/**
 * @brief ORE Studio's parameter value sets, resolved once: the curation
 * generate_value_sets.py writes its seed from, with the convention,
 * curve and engine run settings sets added, and the element map
 * generate_ore_element_map.py derives from the same resolution. Three
 * gates read this model: the value set seed, the element map and
 * check_parameter_value_sets.py.
 */
namespace compass::crosscheck::ops::value_sets {

inline constexpr std::string_view terms_path = "projects/ores.sql/populate/"
    "refdata/refdata_convention_kind_terms_populate.sql";
inline constexpr std::string_view generator_path =
    "projects/ores.sql/utility/generate_value_sets.py";
inline constexpr std::string_view map_generator_path =
    "projects/ores.sql/utility/generate_ore_element_map.py";

/**
 * @brief One value of an enumerated set. A field the curation leaves out
 * is null.
 */
struct value_row {
    json_value value;
    json_value label;
    json_value description;
    json_value order;
    std::optional<json_value> is_active;
    std::optional<json_value> canonical_value;

    bool operator==(const value_row&) const = default;
};

/**
 * @brief A set as the seed states it. An optional field is one the
 * curation may leave out, which the seed writes as its default.
 */
struct value_set {
    json_value name;
    json_value description;
    std::string source_kind;
    std::optional<json_value> cardinality;
    std::optional<json_value> catalogue_code;
    std::optional<json_value> minimum;
    std::optional<json_value> maximum;
    std::optional<json_value> pattern;
    std::optional<json_value> is_ordered;
    std::optional<std::vector<value_row>> values;

    bool operator==(const value_set&) const = default;
};

struct catalogue {
    json_value name;
    json_value relation;
    json_value description;
    std::optional<json_value> is_active;
};

/**
 * @brief The resolved curation. Every table is ordered by its key, the
 * order the seed is written in. A link's key is scope|name.
 */
struct model {
    std::map<std::string, catalogue, std::less<>> catalogues;
    std::map<std::string, value_set, std::less<>> sets;
    std::map<std::string, std::string, std::less<>> links;
    std::set<std::string, std::less<>> generated;
};

/**
 * @brief A column that carries an ORE leaf, and its place among the
 * columns of the same leaf.
 */
struct map_column {
    std::string code;
    std::string map_code;
    std::string schema;
    std::string element;
    std::string entity;
    std::string table;
    std::string column;
    std::string xpath;
    int position = 0;
};

/**
 * @brief One leaf of the element map: the set that governs it, where the
 * rule comes from and why. A leaf whose source is ore_schema names no
 * authority at all.
 */
struct map_row {
    std::string code;
    std::string schema;
    std::string xpath;
    std::string element;
    std::string set;
    std::string name;
    std::string source;
    std::string source_ref;
    std::string reason;
};

/**
 * @brief A problem of the curation, under the rule it breaks.
 */
struct problem {
    std::string rule;
    std::string message;
};

/**
 * @brief Everything the three gates read, from one pass over the inputs.
 *
 * stopped says why the curation cannot be resolved at all, where a
 * generator raises; the model is then empty. problems are what the
 * resolution and the sets' own rules find, sorted by rule and message.
 * The element map is there when the model resolved with no problem, as
 * the map generator requires; otherwise map_stopped says why not.
 * unread_allowed is the Unread leaves allowed table, the leaves that may
 * name no authority.
 */
struct outcome {
    std::optional<std::string> stopped;
    model data;
    std::vector<problem> problems;
    std::set<std::pair<std::string, std::string>> drawn;
    std::optional<std::vector<map_column>> columns;
    std::optional<std::vector<map_row>> rows;
    std::optional<std::string> map_stopped;
    long long unread_allowed = 0;
    std::optional<convention_terms_result> terms;
};

/**
 * @brief Where a codec of the value sets cannot have its tables: the
 * check document to write or correct, and why.
 */
struct unreadable {
    std::string path;
    std::string message;
};

/**
 * @brief The tables of the check document whose Definition names
 * @p codec, from its data option, or where and why they cannot be read.
 *
 * They are what ORE Studio's value set modules kept as module-level
 * literals, and what generate_convention_kind_terms.py did
 * (projection_columns' convention_terms_tables()). From
 * convention_value_sets.py and curve_value_sets.py: Catalogues
 * (CATALOGUES, which both stated alike), Catalogue kinds (CATALOGUE_OF),
 * Rendered kinds (RENDERED_OF), Kinds without a list (WITHOUT_LIST), Kinds
 * one parser lists (LIST_OF), Type sets (each family's TYPE_SETS), Schema
 * type sets (SCHEMA_TYPE_SETS, which both stated alike), Convention own
 * sets (OWN_SETS), Type overrides (TYPE_OVERRIDE), Curation keys
 * (CURATION_KEYS), Curation sources (SOURCES), List items (LIST_ITEM) and
 * Period pattern (PERIOD_PATTERN). From ore_aliases.py: Parser tables
 * (PARSER_TABLE) and Rendering families (SETS). From ore_schema_types.py:
 * Schema kinds (KIND_OF). From generate_ore_element_map.py: Unread leaves
 * allowed (UNREAD_ALLOWED), Item words (ITEM_WORDS), and Schema words
 * (SCHEMA_WORDS and SCHEMA_NAMES, which share their keys). And Value set
 * entities (entities_table), which read_storage() reads.
 */
[[nodiscard]] std::expected<compass::codec::ops::data_tables, unreadable>
read_tables(const compass::codec::types::corpus_view& v,
    std::string_view codec);

/**
 * @brief The role an entity the value sets are stored in plays, in the
 * order of role_names.
 */
enum class role : std::uint8_t {
    vocabulary,
    element_map_column,
    value_set,
    value_set_value,
    value_set_catalogue,
    setting_value_set
};

inline constexpr std::array<std::string_view, 6> role_names{"vocabulary",
    "element_map_column", "value_set", "value_set_value",
    "value_set_catalogue", "setting_value_set"};

inline constexpr std::array<std::string_view, 2> entity_columns{"role",
    "entity"};

/**
 * @brief The table of the check document that names, by role, the entity
 * each part of the value sets is stored in.
 */
inline constexpr compass::codec::ops::table_need entities_table{
    "Value set entities", entity_columns};

/**
 * @brief What the model of one stored entity states: its product (the
 * :product: flag), its component (#+component:), its table (the first
 * :tablename:) and its plural (#+entity_plural:, or the singular with an
 * s, as code generation derives it).
 */
struct stored_entity {
    std::string product;
    std::string component;
    std::string table;
    std::string plural;
};

/**
 * @brief The entities the value sets are stored in, by role, and the
 * names the seeds and the validators derive from them.
 */
struct storage {
    std::array<stored_entity, role_names.size()> entities;

    [[nodiscard]] const stored_entity& operator[](role r) const;

    /**
     * @brief projects/<product>.sql/populate/<component>/<component>_
     * <plural>_populate.sql, the seed of @p r's entity, where code
     * generation puts an entity's scripts.
     */
    [[nodiscard]] std::string seed_path(role r) const;

    /**
     * @brief <product>_<component>_service, the writer a seed of @p r's
     * entity stamps.
     */
    [[nodiscard]] std::string writer(role r) const;

    /**
     * @brief <product>_<component>_<stem>_fn, a function of the component
     * that owns the value set entity.
     */
    [[nodiscard]] std::string function(std::string_view stem) const;

    /**
     * @brief @p text with {{vocabulary_table}},
     * {{element_map_column_table}}, {{value_in_set_fn}} and
     * {{canonical_value_fn}} replaced by their names.
     */
    [[nodiscard]] std::string fill(std::string_view text) const;
};

/**
 * @brief The storage the Value set entities table of @p data names: each
 * role's entity, found by name as the one model <anything>.<entity>.org
 * under a modeling/ directory of projects/. A role with no row, or an
 * entity with no model or several, is unreadable at the table's line.
 */
[[nodiscard]] std::expected<storage, unreadable> read_storage(
    const compass::codec::types::corpus_view& v,
    const compass::codec::ops::data_tables& data, std::string_view codec);

/**
 * @brief Resolves the value sets and the element map from the checkout
 * and the tables read_tables() read.
 */
[[nodiscard]] outcome evaluate(const compass::codec::types::corpus_view& v,
    const compass::codec::ops::data_tables& data);

/**
 * @brief generate_convention_kind_terms.py's three files, the convention
 * kind terms seed, their validation triggers and the triggers' drops, from
 * @p terms, or a refusal of the seed that says why not.
 */
void write_convention_kind_terms(const compass::codec::types::corpus_view& v,
    const std::optional<convention_terms_result>& terms, const storage& names,
    compass::codec::types::file_output& out);

/**
 * @brief The value set seed, as generate_value_sets.render() writes it.
 */
[[nodiscard]] std::string render_seed(const model& m, const storage& names);

/**
 * @brief The element map seed and its columns seed, as
 * generate_ore_element_map.py writes them.
 */
[[nodiscard]] std::string render_map(const std::vector<map_row>& rows,
    const storage& names);
[[nodiscard]] std::string render_columns(
    const std::vector<map_column>& columns, const storage& names);

}

#endif
