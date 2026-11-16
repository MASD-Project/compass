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
#ifndef COMPASS_CODEGEN_OPS_RENDER_HPP
#define COMPASS_CODEGEN_OPS_RENDER_HPP

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include "compass.codegen/types/compiled_template.hpp"
#include "compass.model/types/model_tables.hpp"

namespace compass::codegen::ops {

/**
 * @brief How {{name}} escapes a value. ores.codegen renders with no
 * escaping; pystache's default is html.escape(value, quote=True).
 */
enum class escaping : std::uint8_t { none, html };

/**
 * @brief The source of a partial by its name, or nothing when there is
 * none, which renders as nothing.
 */
using partial_finder =
    std::function<std::optional<std::string_view>(std::string_view name)>;

struct render_options {
    escaping escape = escaping::none;
    partial_finder partials;
    /**
     * @brief Whether an object with a cpp_type answers cpp_value_init with
     * "{}" when the type is a C++ scalar and "" otherwise, whatever it
     * holds, as ores.codegen's _mark_value_init sets it before rendering a
     * template that declares members.
     */
    bool value_init = false;
};

/**
 * @brief A value of some model tables. The bottom of a context stack can
 * mix tables: a context and an object of values that shadow some of its
 * keys for one render.
 */
struct value_ref {
    const compass::model::types::model_tables* tables = nullptr;
    compass::model::types::value_id value;
};

/**
 * @brief Appends @p t rendered against the context stack @p stack, whose
 * last value is its top.
 *
 * The semantics are pystache's. A name is looked up in the objects on the
 * context stack from the top down; a dotted name looks up its first part so
 * and the rest in what that finds. A name nothing holds renders as nothing.
 * A value renders as Python's str() does: True, False and None for a
 * boolean and null, and repr() for the items of a list or an object.
 * A section repeats for each item of a list and renders once, with the value
 * pushed, for any other value Python finds true; an empty object is false.
 * A number that the model holds as text, such as a JSON float, is true even
 * when it is zero.
 */
void render(std::string& out, const types::compiled_template& t,
    std::span<const value_ref> stack, const render_options& options = {});

/**
 * @brief Appends @p t rendered against the one context @p context.
 */
void render(std::string& out, const types::compiled_template& t,
    const compass::model::types::model_tables& values,
    compass::model::types::value_id context,
    const render_options& options = {});

}

#endif
