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
#ifndef COMPASS_CODEGEN_OPS_GENERATE_HPP
#define COMPASS_CODEGEN_OPS_GENERATE_HPP

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/file_table.hpp"
#include "compass.codegen/types/compiled_template.hpp"
#include "compass.model/ops/load.hpp"
#include "compass.model/types/model_tables.hpp"

namespace compass::codegen::ops {

/**
 * @brief Everything generation reads of a checkout. Every read goes
 * through it, so a host that records reads, such as a codec's view, sees
 * them all.
 * find gives a parsed document by path. list gives the paths of the files,
 * documents or not, that start with a prefix and end with a suffix, in
 * byte order; the views stay valid while generation runs. read gives the
 * bytes of a file and exists says whether there is one at a path.
 */
struct checkout_reader {
    compass::model::ops::document_finder find;
    std::function<std::vector<std::string_view>(std::string_view prefix,
        std::string_view suffix)> list;
    std::function<std::optional<std::string>(std::string_view path)> read;
    std::function<bool(std::string_view path)> exists;
};

/**
 * @brief Where ores.codegen's library is and what it is told.
 * library is the directory, relative to the checkout, that holds
 * templates/, data/ and component_catalogue.org.
 * convention_component names the component of the catalogue whose
 * modeling directory declares the convention families. year is the
 * copyright year the licence block states.
 */
struct generation_settings {
    std::string library;
    std::string convention_component;
    int year = 0;
};

/**
 * @brief Every .mustache template of the library, compiled once, by file
 * name.
 */
using template_map = std::map<std::string, types::compiled_template,
    std::less<>>;

/**
 * @brief One output to render: its path, its template and the context it
 * renders from, a value of the plan's tables, with ores.codegen's name for
 * the type of its model.
 */
struct planned_output {
    std::string output;
    std::string template_name;
    compass::model::types::value_id context;
    std::string_view type;
};

/**
 * @brief What regenerate --all would render: one row per output path,
 * with every context in tables. contexts counts the model contexts built;
 * messages names each model that failed to load.
 */
struct generation_plan {
    compass::model::types::model_tables tables;
    std::vector<planned_output> outputs;
    std::size_t contexts = 0;
    std::vector<std::string> messages;
};

/**
 * @brief Compiles the .mustache files of the templates/ directory of the
 * library @p s names.
 * @throws std::runtime_error when a listed template cannot be read.
 */
[[nodiscard]] template_map compile_templates(const checkout_reader& reader,
    const generation_settings& s);

/**
 * @brief Plans into @p plan the outputs of the models regenerate --all
 * visits whose types compass builds contexts for, each with its context,
 * in the order resolve_targets lists them. An output that a later model
 * names again is rendered from the later context, as the later write wins
 * on disk.
 * @throws std::runtime_error when the library's catalogue or data files
 * cannot be read, or the convention component is not in the catalogue.
 */
void plan_generation(generation_plan& plan, const checkout_reader& reader,
    const generation_settings& s);

/**
 * @brief Renders @p output of @p plan into @p text with its template.
 * @throws std::runtime_error when @p templates has no such template.
 */
void render_planned(std::string& text, const template_map& templates,
    const generation_plan& plan, const planned_output& output);

/**
 * @brief Plans, compiles and renders every output, adding each to
 * @p out.
 * @throws std::runtime_error naming the models that failed to load, and
 * as plan_generation and render_planned do.
 */
void generate(compass::codec::types::file_table& out,
    const checkout_reader& reader, const generation_settings& s);

}

#endif
