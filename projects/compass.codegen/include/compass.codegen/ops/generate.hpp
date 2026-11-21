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
#include "compass.codec/types/descriptor.hpp"
#include "compass.codegen/ops/facts.hpp"
#include "compass.codegen/types/compiled_template.hpp"
#include "compass.codegen/types/entity_registry.hpp"
#include "compass.codegen/types/physical_space.hpp"
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
 * @brief What regenerate --all would render, and the outputs of the
 * service registry beside them: one row per output path,
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
 * @brief What the generation of every model shares, read once: the
 * settings, the library's licence and modeline texts, the physical-space
 * graph, the two registries read from every model of the checkout, and
 * the compiled templates.
 *
 * models lists, in the order resolve_targets visits them, the models
 * regenerate --all generates whose types compass builds contexts for,
 * and every service registry of a modeling directory, which no
 * component's catalogue entry reaches and generate --model renders.
 * facts holds what a model's generation may depend on besides the
 * documents and files it reads itself: library_fact, graph_fact, one
 * fact per template (template_fact) and those of the registries
 * (publish_facts).
 */
struct generation {
    generation_settings settings;
    std::string licence;
    std::string modeline;
    types::physical_space graph;
    types::checkout_registry registry;
    types::entity_registry entities;
    template_map templates;
    std::vector<std::string> models;
    std::vector<fact> facts;
};

/**
 * @brief The fact that is the library's licence and modeline texts and
 * the copyright year, and the fact that is every node of the
 * physical-space graph.
 */
inline constexpr std::string_view library_fact = "library";
inline constexpr std::string_view graph_fact = "graph";

/**
 * @brief The name of the fact that is the bytes of the template whose
 * file is named @p name.
 */
[[nodiscard]] std::string template_fact(std::string_view name);

/**
 * @brief Compiles the .mustache files of the templates/ directory of the
 * library @p s names.
 * @throws std::runtime_error when a listed template cannot be read.
 */
[[nodiscard]] template_map compile_templates(const checkout_reader& reader,
    const generation_settings& s);

/**
 * @brief Reads what every model's generation shares.
 * @throws std::runtime_error when the library's catalogue, data files or
 * templates cannot be read, or the convention component is not in the
 * catalogue.
 */
[[nodiscard]] generation prepare_generation(const checkout_reader& reader,
    const generation_settings& s);

/**
 * @brief Plans into @p plan the outputs of every model of
 * prepare_generation, each with its context, in order. An output that a
 * later model names again is rendered from the later context, as the
 * later write wins on disk.
 * @throws std::runtime_error as prepare_generation does.
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
 * @brief Generates the outputs of the model at @p model, one of
 * @p g's models, into @p out, in the order its units name them. It reads
 * the model and what the model reads through @p reader, and tells
 * @p out each fact of @p g it depended on: the library and the graph,
 * the templates of its outputs, and the elements of the registries it
 * looked up.
 *
 * When @p out says the only inputs that changed since the model's last
 * generation are templates, the outputs of the other templates are kept
 * as they were and their contexts are not built.
 *
 * @p cache spares parsing what several models read; one cache serves the
 * models generated on one thread.
 * @throws std::runtime_error when the model does not load or an output
 * needs a template the library does not have.
 */
void generate_model(const generation& g, std::string_view model,
    const checkout_reader& reader, compass::model::ops::load_cache& cache,
    compass::codec::types::part_sink& out);

}

#endif
