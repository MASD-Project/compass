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
#ifndef COMPASS_MODEL_OPS_LOAD_HPP
#define COMPASS_MODEL_OPS_LOAD_HPP

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.model/ops/builder.hpp"
#include "compass.model/ops/outline.hpp"
#include "compass.model/types/model_tables.hpp"
#include "compass.org/types/tables.hpp"

namespace compass::model::ops {

/**
 * @brief One parsed document: its path relative to the root of the
 * checkout, its bytes and its tables.
 */
struct document {
    std::string_view path;
    std::string_view source;
    const compass::org::types::tables_view* tables = nullptr;
};

/**
 * @brief Finds another document of the checkout by its path relative to the
 * root, or nothing when there is none.
 *
 * A model may read other documents: the profile it names, the field groups
 * it includes. Every such read goes through the finder, so a host that
 * records reads, such as a codec's corpus view, sees them all.
 */
using document_finder =
    std::function<std::optional<document>(std::string_view path)>;

/**
 * @brief The paths, relative to the root, of the documents of the checkout
 * that start with @p prefix, in any order.
 *
 * A model that reads the models beside it, as a family reads its entities
 * and the trade type catalogue reads every entity of projects/, finds them
 * through the lister, as ores.codegen globs the directories.
 */
using document_lister =
    std::function<std::vector<std::string_view>(std::string_view prefix)>;

/**
 * @brief A model ores.codegen refuses to load, as its ValueError.
 */
class model_error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief What a run of loads parses once and reuses: the custom type
 * registry and the profile tables, as ores.codegen caches them for a run.
 *
 * Every model that needs one still asks the finder for its document, so a
 * finder that records reads sees each read; the cache only spares parsing
 * the same bytes again. An entry is reused while the finder hands out the
 * same bytes for its path.
 */
class load_cache final {
public:
    load_cache();
    ~load_cache();
    load_cache(const load_cache&) = delete;
    load_cache& operator=(const load_cache&) = delete;

    struct impl;
    [[nodiscard]] impl& get() noexcept { return *impl_; }

private:
    std::unique_ptr<impl> impl_;
};

/**
 * @brief The model type a #+type: value names, or nothing when it is not a
 * type ores.codegen's load_model loads.
 */
[[nodiscard]] std::optional<types::model_type> model_type_of(
    std::string_view type) noexcept;

/**
 * @brief The #+type: value of a parsed document, or empty.
 */
[[nodiscard]] std::string_view declared_type(const document& d);

/**
 * @brief Loads @p d as a model of type @p t, with the value
 * ores.codegen.core.load_model returns for it, and returns its row.
 *
 * Where ores.codegen raises ValueError the model's value is
 * {"error": "ValueError"}, which is what codegen_model_dump.py records for
 * it, and @p error, when given, gets the message.
 */
types::model_id load_model(builder& b, const document& d, types::model_type t,
    const document_finder& find, const document_lister& list,
    load_cache& cache, std::string* error = nullptr);

/**
 * @brief The path of the field group @p group that the model at
 * @p model_path names, as _resolve_domain_group_fields finds it: beside
 * the model when @p find finds it there, else, for a name of three
 * segments, under the modeling directory of the component its first two
 * name.
 */
[[nodiscard]] std::string field_group_path(std::string_view model_path,
    std::string_view group, const document_finder& find);

/**
 * @brief read_physical_space_overrides for the model whose outline is
 * @p o: the | Address | Enabled | rows of the * Physical space tables of
 * the profiles its * Flags drawer binds, then of its own, each as
 * (<address>.enabled, enabled). A later pair overrides an earlier one, so
 * the model's own table wins.
 *
 * Throws model_error when a bound profile's document does not exist.
 */
[[nodiscard]] std::vector<std::pair<std::string, bool>>
physical_space_overrides(const outline& o, const document_finder& find,
    load_cache& cache);

}

#endif
