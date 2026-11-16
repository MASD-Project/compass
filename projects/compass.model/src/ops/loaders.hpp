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
#ifndef COMPASS_MODEL_OPS_LOADERS_HPP
#define COMPASS_MODEL_OPS_LOADERS_HPP

#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.model/ops/builder.hpp"
#include "compass.model/ops/economic.hpp"
#include "compass.model/ops/load.hpp"
#include "compass.model/ops/outline.hpp"
#include "compass.model/ops/typescript.hpp"

/*
 * One loader per org_loader function of ores.codegen. Each fills the root
 * object of one model.
 */
namespace compass::model::ops {

/**
 * @brief One literal row of a profile's Assignments table: a boolean or a
 * text default for a feature.
 */
struct profile_assignment {
    std::string feature;
    bool is_bool = false;
    bool flag = false;
    std::string text;
};

struct load_cache::impl {
    template <typename T>
    struct entry {
        const char* source = nullptr;
        std::size_t size = 0;
        T value;
    };
    std::map<std::string, entry<custom_types>, std::less<>> registries;
    std::map<std::string, entry<std::vector<profile_assignment>>, std::less<>>
        profiles;
    std::map<std::string, entry<std::vector<std::pair<std::string, bool>>>,
        std::less<>> physical_spaces;
};

struct load_context {
    builder& b;
    const document& d;
    const outline& o;
    const document_finder& find;
    load_cache& cache;
};

/**
 * @brief {kind UUID: [code, ...]} of every source block with :implements,
 * in document order, as _collect_implementations builds it; nothing is
 * written when there is none.
 */
void collect_implementations(load_context& c, types::value_id parent,
    std::string_view key);

/**
 * @brief The value of frontmatter @p key, or @p fallback.
 */
[[nodiscard]] std::string_view frontmatter_or(const outline& o,
    std::string_view key, std::string_view fallback = {});

/**
 * @brief The file name of the document: the last component of its path.
 */
[[nodiscard]] std::string_view file_name(const document& d) noexcept;

/**
 * @brief int() of Python over a property value: an optional sign and
 * decimal digits, single underscores allowed between them.
 */
[[nodiscard]] std::optional<std::int64_t> parse_int(std::string_view s)
    noexcept;

/**
 * @brief What _parse_typed makes of @p raw: a boolean for true or false in
 * any case, an integer when int() reads it, or else the string.
 */
[[nodiscard]] types::value_kind typed_kind(std::string_view raw) noexcept;
void set_typed(builder& b, types::value_id parent, std::string_view key,
    std::string_view raw);

[[nodiscard]] std::vector<std::string_view> lines_of(std::string_view text);

/**
 * @brief The #include targets of the block named includes of @p n, as
 * _includes_from_named_block reads them, appended to @p list.
 */
void includes_from_named_block(load_context& c, node_id n,
    types::value_id list);

/**
 * @brief _ensure_profile_binding: refuses :profile: outside * Flags.
 */
void ensure_profile_binding(const outline& o);

/**
 * @brief _reject_junction_only_flags: refuses :client_read_only: in the
 * root * Flags, unless @p root_allowed, and in * C++ ** Flags.
 */
void reject_junction_only_flags(const outline& o, std::string_view kind,
    bool root_allowed = false);

/**
 * @brief Writes has_economic_declaration, economic_fields and
 * has_economic_fields and, when @p canonical, canonical_members and
 * has_canonical_members as the same list.
 */
void write_economic(builder& b, types::value_id parent, const economic& e,
    bool canonical);

/**
 * @brief str() of Python over a scalar value.
 */
[[nodiscard]] std::string text_value(const builder& b, types::value_id v);

/**
 * @brief The Python truth of a value: true, a non-zero integer, a non-empty
 * string, list or object.
 */
[[nodiscard]] bool truthy(const builder& b, types::value_id v);

/**
 * @brief The text of a string value, or empty for any other value or none.
 */
[[nodiscard]] std::string_view string_of(const builder& b, types::value_id v);

/**
 * @brief One row of an org table as _normalise_table makes it: a dict from
 * header to cell, a header written twice keeping its first place and its
 * last cell.
 */
struct org_row {
    std::vector<key_value> cells;

    [[nodiscard]] lookup get(std::string_view key) const;
    [[nodiscard]] std::string_view value_or(std::string_view key,
        std::string_view fallback = {}) const;
};

/**
 * @brief The rows below the header of table @p table.
 */
[[nodiscard]] std::vector<org_row> rows_of(const outline& o,
    std::uint32_t table);

/**
 * @brief _parse_org_table_rows: the rows of the first table of @p n or, when
 * it has none, of its first descendant that has one.
 */
[[nodiscard]] std::vector<org_row> first_table_rows(const outline& o,
    node_id n);

/**
 * @brief The rows of the first table of @p n itself, or none.
 */
[[nodiscard]] std::vector<org_row> own_table_rows(const outline& o,
    node_id n);

/**
 * @brief Every property of @p n with its key lowered, typed by
 * _parse_typed unless its key is in @p raw.
 */
void set_properties(builder& b, types::value_id object, const outline& o,
    node_id n, std::initializer_list<std::string_view> raw = {});

/**
 * @brief The org_id of a node: its last property named ID in any case.
 */
[[nodiscard]] lookup org_id(const outline& o, node_id n);

/**
 * @brief One row of _TS_UTILITY_DOMAIN_TYPES: a C++ type with a
 * hand-written TypeScript interface, and the module that exports it.
 */
struct ts_utility_type {
    std::string_view cpp;
    std::string_view pascal;
    std::string_view module;
};
[[nodiscard]] std::span<const ts_utility_type> ts_utility_types() noexcept;

/**
 * @brief parse_declared_messages: the * Messages section as the list both
 * twins render, appended to @p list.
 */
void declared_messages(load_context& c, types::value_id list);

void load_component(load_context& c, types::value_id root);
void load_component_overview(load_context& c, types::value_id root);
void load_field_group(load_context& c, types::value_id root);
void load_family(load_context& c, types::value_id root);
void load_entity(load_context& c, types::value_id root);
void load_operation(load_context& c, types::value_id root);
void load_outcome_catalogue(load_context& c, types::value_id root);
void load_asset_class_catalogue(load_context& c, types::value_id root);
void load_service_registry(load_context& c, types::value_id root);
void load_junction(load_context& c, types::value_id root);

}

#endif
