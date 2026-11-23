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
#include "compass.codegen/codecs/codegen.hpp"

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include "compass.codegen/ops/context.hpp"
#include "compass.codegen/ops/library.hpp"
#include "compass.codegen/ops/output.hpp"
#include "compass.platform/process/executable.hpp"

namespace compass::codegen::codecs {

namespace {

namespace ct = compass::codec::types;

constexpr std::array<ct::option, 8> options{{
    {"library", "", "The directories of the library, comma-separated, in "
        "order: @compass, the MASD library installed with compass; a path "
        "relative to the checkout; or an absolute path. Together they hold "
        "templates/, data/ and component_catalogue.org. A template of a "
        "later directory replaces one of the same name in an earlier one; "
        "a data file is read from the last that has it; a node in two is "
        "an error."},
    {"product", "", "The project's product: the root of every "
        "physical-space address, the prefix of its component directories "
        "and the templates' product."},
    {"convention_component", "", "The component of the catalogue whose "
        "modeling directory declares the convention families."},
    {"year", "", "The copyright year of the licence blocks. Required: the "
        "year is configuration, never the clock."},
    {"clang_format", "", "The clang-format that formats C++ outputs: "
        "comma-separated candidates, the first found used; empty for none."},
    {"clang_format_style", "", "clang-format's style file, relative to the "
        "checkout."},
    {"prettier", "", "The prettier that formats TypeScript outputs: "
        "comma-separated candidates, the first found used; empty for none."},
    {"prettier_style", "", "prettier's configuration file, relative to the "
        "checkout."}}};

std::string required(const ct::option_values& o, std::string_view key) {
    const auto value = o[key];
    if (value.empty())
        throw std::runtime_error("codegen: set the option " +
            std::string(key));
    return std::string(value);
}

/**
 * @brief The first of the comma-separated @p candidates that is a
 * program: a name on PATH, or a path relative to @p checkout.
 */
std::optional<std::filesystem::path> first_program(
    std::string_view candidates, const std::filesystem::path& checkout) {
    while (!candidates.empty()) {
        const auto comma = candidates.find(',');
        const auto name = candidates.substr(0, comma);
        candidates = comma == std::string_view::npos ? std::string_view{}
            : candidates.substr(comma + 1);
        const auto found = name.find('/') == std::string_view::npos
            ? compass::platform::process::find_program(name)
            : compass::platform::process::find_program(
                (checkout / name).generic_string());
        if (found)
            return found;
    }
    return std::nullopt;
}

/**
 * @brief The formatter @p key names, with the program and style its
 * options give, or nothing when the program option is empty.
 */
std::optional<ct::formatter> configured(const ct::option_values& o,
    const std::filesystem::path& checkout, std::string_view key,
    ct::formatter f) {
    const auto candidates = o[key];
    if (candidates.empty())
        return std::nullopt;
    const auto program = first_program(candidates, checkout);
    if (!program)
        throw std::runtime_error("codegen: none of " +
            std::string(candidates) + " is a program, for the option " +
            std::string(key));
    f.program = *program;
    f.style = checkout / required(o, std::string(key) + "_style");
    return f;
}

std::shared_ptr<const void> prepare(const ct::corpus_view& v,
    ct::stage& out) {
    auto g = std::make_shared<ops::generation>(ops::prepare_generation(
        reader_of(v), settings_of(v.options())));
    out.parts = g->models;
    out.facts = g->facts;
    return g;
}

void run(const void* state, std::span<const std::string_view> models,
    const ct::corpus_view& v, ct::part_sink& out) {
    const auto& g = *static_cast<const ops::generation*>(state);
    const auto reader = reader_of(v);
    compass::model::ops::load_cache cache;
    for (const auto model : models) {
        out.begin(model);
        ops::generate_model(g, model, reader, cache, out);
    }
}

/**
 * @brief The template the marker of a generated file names, under the
 * templates/ directory of the last library that holds it. A library of
 * the checkout is looked into under @p checkout.
 */
std::string origin(const ct::option_values& o, std::string_view content,
    const std::filesystem::path& checkout) {
    const auto name = ops::marked_template(content);
    if (name.empty())
        return {};
    std::vector<std::string> libraries;
    try {
        libraries = ops::library_directories(o["library"],
            compass::platform::process::executable_path());
    } catch (const std::runtime_error&) {
        return {};
    }
    for (auto it = libraries.rbegin(); it != libraries.rend(); ++it) {
        const auto path = *it + "/templates/" + std::string(name);
        const auto on_disk = ops::outside_the_checkout(*it)
            ? std::filesystem::path(path) : checkout / path;
        std::error_code ec;
        if (std::filesystem::is_regular_file(on_disk, ec))
            return path;
    }
    return {};
}

/**
 * @brief Every file of the libraries outside the checkout, which no run
 * records as an input.
 */
std::string environment(const ct::option_values& o) {
    try {
        return ops::outside_files_digest(ops::library_directories(
            o["library"], compass::platform::process::executable_path()));
    } catch (const std::runtime_error&) {
        return {};
    }
}

}

ops::generation_settings settings_of(const ct::option_values& o) {
    return settings_of(o, compass::platform::process::executable_path());
}

ops::generation_settings settings_of(const ct::option_values& o,
    const std::filesystem::path& executable) {
    ops::generation_settings s;
    s.libraries = ops::library_directories(required(o, "library"),
        executable);
    for (const auto& library : s.libraries) {
        std::error_code ec;
        if (ops::outside_the_checkout(library) &&
            !std::filesystem::is_directory(library, ec))
            throw std::runtime_error("codegen: the library " + library +
                " is not a directory");
    }
    s.product = required(o, "product");
    s.convention_component = required(o, "convention_component");
    s.year = ops::copyright_year("codegen", o["year"]);
    return s;
}

std::vector<ct::formatter> formatters_of(const ct::option_values& o,
    const std::filesystem::path& checkout) {
    std::vector<ct::formatter> out;
    if (auto f = configured(o, checkout, "clang_format",
            {.name = "clang-format", .program = {},
             .extensions = {".cpp", ".hpp", ".h", ".cxx", ".cc"},
             .arguments = {"-i", "--style=file:{style}"}, .passes = 2,
             .style = {}}))
        out.push_back(std::move(*f));
    if (auto f = configured(o, checkout, "prettier",
            {.name = "prettier", .program = {}, .extensions = {".ts", ".tsx"},
             .arguments = {"--write", "--log-level", "warn", "--config",
                 "{style}"},
             .passes = 1, .style = {}}))
        out.push_back(std::move(*f));
    return out;
}

ops::checkout_reader reader_of(const ct::corpus_view& v) {
    return ops::with_files_outside({
        .find = [&v](std::string_view path)
            -> std::optional<compass::model::ops::document> {
            const auto d = v.find_path(path);
            if (!d)
                return std::nullopt;
            return compass::model::ops::document{d->path, d->source,
                &d->tables};
        },
        .list = [&v](std::string_view prefix, std::string_view suffix) {
            return v.list_files(prefix, suffix);
        },
        .read = [&v](std::string_view path) { return v.read_file(path); },
        .exists = [&v](std::string_view path) {
            return v.file_exists(path);
        }});
}

const ct::descriptor codegen{
    .name = "codegen",
    .version = 4,
    .summary = "Generates what ores.codegen's regenerate --all writes, "
        "and the outputs of the service registry.",
    .reads = ct::inputs::every_document(),
    .options = options,
    .rules = {},
    .enabled_by_default = false,
    .run = ct::staged_function{prepare, run},
    .formatters = formatters_of,
    .origin = origin,
    .environment = environment};

}
