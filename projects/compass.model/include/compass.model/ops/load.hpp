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
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include "compass.model/ops/builder.hpp"
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
 * @brief A model ores.codegen refuses to load, as its ValueError.
 */
class model_error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
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
    const document_finder& find, std::string* error = nullptr);

}

#endif
