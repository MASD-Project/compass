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
#include "compass.codegen/codecs/component_files.hpp"

#include <array>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codegen/ops/compile.hpp"
#include "compass.codegen/ops/context.hpp"
#include "compass.codegen/ops/output.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::codegen::codecs {

namespace {

namespace ct = compass::codec::types;
namespace mops = compass::model::ops;
namespace pt = compass::platform::text;

constexpr std::array<ct::option, 2> options{{
    {"library", "projects/ores.codegen/library", "The codegen library: its "
        "data directory and its templates."},
    {"year", "", "The copyright year of the licence blocks. Required: the "
        "year is configuration, never the clock."}}};

std::string read(const ct::corpus_view& v, const std::string& path) {
    auto bytes = v.read_file(path);
    if (!bytes)
        throw std::runtime_error("component_files: cannot read " + path);
    return std::move(*bytes);
}

/**
 * @brief The paths under @p directory ending in @p suffix, relative to it,
 * in byte order.
 */
void list_under(const ct::corpus_view& v, mops::builder& b,
    compass::model::types::value_id list, const std::string& directory,
    std::string_view suffix) {
    for (const auto path : v.list_files(directory, suffix))
        b.append(list, path.substr(directory.size()));
}

void run(const ct::corpus_view& v, ct::file_output& out) {
    static const pt::regex full_name(R"(^#\+full_name:\s*(\S+)\s*$)",
        {.multiline = true});
    std::map<std::string, std::string, std::less<>> models;
    for (const auto path :
        v.list_files("projects/", "/modeling/component_overview.org")) {
        const auto text = read(v, std::string(path));
        if (const auto m = full_name.search(text))
            models.insert_or_assign(std::string(m->group(1)),
                std::string(path));
    }
    if (models.empty())
        return;

    const std::string library(v.option("library"));
    const auto year = ops::copyright_year("component_files",
        v.option("year"));
    const auto licence = read(v, library + "/data/licence-GPL-v3.txt");
    const auto modelines = read(v, library + "/data/modeline.org");
    struct unit {
        std::string_view directory;
        std::string template_name;
        types::compiled_template compiled;
    };
    std::array<unit, 2> units{{{"src", "cmake_component_files_src.mustache",
        {}}, {"tests", "cmake_component_files_tests.mustache", {}}}};
    for (auto& u : units)
        u.compiled = ops::compile(read(v,
            library + "/templates/" + u.template_name));

    for (const auto& [name, model] : models) {
        const auto modeling = model.substr(0, model.rfind('/'));
        const auto root = modeling.substr(0, modeling.rfind('/') + 1);
        for (const auto& u : units) {
            const auto directory = root + std::string(u.directory) + "/";
            if (!v.file_exists(directory + "CMakeLists.txt"))
                continue;
            compass::model::types::model_tables tables;
            mops::builder b(tables);
            const auto context = ops::library_values(b, licence, modelines,
                year);
            const auto component = b.object(context, "component");
            list_under(v, b, b.list(component, "files"), directory, ".cpp");
            const auto headers = b.list(component, "headers");
            list_under(v, b, headers, root + "include/", ".hpp");
            b.set(component, "has_headers",
                !tables.values.firsts[headers].is_none());
            std::string text;
            ops::render_output(text, u.template_name, u.compiled, tables,
                context);
            out.files.add(directory + "component_files.cmake", text);
        }
    }
}

}

const ct::descriptor component_files{
    .name = "component_files",
    .version = 2,
    .summary = "Writes the component_files.cmake source and header lists of "
        "each modelled component.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = {},
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{run}};

}
