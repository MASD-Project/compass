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
#include "compass.crosscheck/codecs/xml_bindings.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.crosscheck/ops/xml_bindings.hpp"
#include "compass.platform/process/executable.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace cops = compass::codec::ops;
namespace xb = ops::xml_bindings;

constexpr std::string_view codec_name = "xml_bindings";
constexpr std::string_view check_document = "compass/checks/xml_bindings.org";

constexpr std::uint32_t unreadable = 1;

constexpr std::array<ct::rule, 2> rules{{
    co::refusal_rules[0],
    {"unreadable", "The check document gives every table, column and "
        "setting the generator reads, and its rows agree with each "
        "other."}}};

constexpr std::array<std::string_view, 4> family_columns{"stem",
    "root element", "root type", "schemas"};
constexpr std::array<std::string_view, 2> exact_decimal_columns{"family",
    "element"};
constexpr std::array<std::string_view, 2> errata_columns{"written", "read"};
constexpr std::array<std::string_view, 2> setting_columns{"setting",
    "value"};
constexpr std::array<std::string_view, 1> licence_columns{"line"};

constexpr std::array<cops::table_need, 5> needed{{
    {"Families", family_columns},
    {"Exact decimals", exact_decimal_columns},
    {"Schema errata", errata_columns},
    {"Settings", setting_columns},
    {"Licence", licence_columns}}};

/**
 * @brief The Settings table of the check document: where the schemas are,
 * where the files go, and what they name of the project.
 */
struct settings {
    std::string schema_directory;
    std::string header_directory;
    std::string source_directory;
    std::string include_prefix;
    std::string namespace_prefix;
    std::string engine_include;
    std::string decimal_include;
    std::string decimal_type;
    std::string export_macro;
};

/**
 * @brief One setting: its name in the table, where it goes, and whether
 * an empty value is one.
 */
struct setting_key {
    std::string_view name;
    std::string settings::* member;
    bool may_be_empty = false;
};

const std::array<setting_key, 9> setting_keys{{
    {"schema_directory", &settings::schema_directory},
    {"header_directory", &settings::header_directory},
    {"source_directory", &settings::source_directory},
    {"include_prefix", &settings::include_prefix},
    {"namespace_prefix", &settings::namespace_prefix},
    {"engine_include", &settings::engine_include},
    {"decimal_include", &settings::decimal_include},
    {"decimal_type", &settings::decimal_type},
    {"export_macro", &settings::export_macro, true}}};

/**
 * @brief One row of the Families table, with the rows of the Exact
 * decimals table that name it.
 */
struct family_row {
    std::string stem;
    std::string root_element;
    std::string root_type;
    std::vector<std::string> schemas;
    std::vector<std::string> exact_decimals;
};

/**
 * @brief Everything the check document states.
 */
struct project_data {
    settings paths;
    std::vector<family_row> families;
    std::vector<std::pair<std::string, std::string>> errata;
    std::string licence;
};

std::vector<std::string> words_of(std::string_view text) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < text.size()) {
        const auto begin = text.find_first_not_of(' ', at);
        if (begin == std::string_view::npos)
            break;
        auto end = text.find(' ', begin);
        if (end == std::string_view::npos)
            end = text.size();
        out.emplace_back(text.substr(begin, end - begin));
        at = end;
    }
    return out;
}

bool is_identifier(std::string_view s) {
    return !s.empty() && std::ranges::all_of(s, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_';
    });
}

/**
 * @brief @p prefix and @p name joined by @p separator, which @p prefix
 * may already end with.
 */
std::string under(std::string_view prefix, std::string_view separator,
    std::string_view name) {
    if (prefix.empty() || prefix.ends_with(separator))
        return std::string(prefix) + std::string(name);
    return std::string(prefix) + std::string(separator) + std::string(name);
}

/**
 * @brief What the check document states, or why it states it wrongly,
 * written for the reader of the finding.
 */
std::expected<project_data, std::string> read_document(
    const cops::data_tables& data) {
    const auto wrong = [&](std::string_view table, std::uint32_t line,
                           const std::string& what) {
        return std::unexpected(std::format("{}:{}: {}, in the table \"{}\" "
            "{} reads", data.source, line, what, table, codec_name));
    };
    project_data out;

    const auto& s = *data.find("Settings");
    std::set<std::string, std::less<>> seen;
    for (std::size_t r = 0; r < s.rows.size(); ++r) {
        const auto& name = s.rows[r][s.column("setting")];
        const auto key = std::ranges::find(setting_keys, name,
            &setting_key::name);
        if (key == setting_keys.end())
            return wrong(s.name, s.row_lines[r], "the setting '" + name +
                "' is not one " + std::string(codec_name) + " has");
        if (!seen.insert(name).second)
            return wrong(s.name, s.row_lines[r], "the setting '" + name +
                "' is stated twice");
        const auto& value = s.rows[r][s.column("value")];
        if (value.empty() && !key->may_be_empty)
            return wrong(s.name, s.row_lines[r], "the setting '" + name +
                "' has no value");
        out.paths.*(key->member) = value;
    }
    for (const auto& key : setting_keys)
        if (!seen.contains(key.name))
            return wrong(s.name, s.line, "no row states the setting '" +
                std::string(key.name) + "'");

    const auto& f = *data.find("Families");
    std::map<std::string, std::size_t, std::less<>> index;
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        const auto& row = f.rows[r];
        family_row family{.stem = row[f.column("stem")],
            .root_element = row[f.column("root element")],
            .root_type = row[f.column("root type")],
            .schemas = words_of(row[f.column("schemas")])};
        if (!is_identifier(family.stem))
            return wrong(f.name, f.row_lines[r], "the stem '" + family.stem +
                "' is not a C++ identifier");
        if (family.root_element.empty() || family.root_type.empty() ||
            family.schemas.empty())
            return wrong(f.name, f.row_lines[r], "the family " + family.stem +
                " lacks its root element, root type or schemas");
        if (!index.emplace(family.stem, out.families.size()).second)
            return wrong(f.name, f.row_lines[r], "the family " + family.stem +
                " is stated twice");
        out.families.push_back(std::move(family));
    }

    const auto& d = *data.find("Exact decimals");
    for (std::size_t r = 0; r < d.rows.size(); ++r) {
        const auto& family = d.rows[r][d.column("family")];
        const auto& element = d.rows[r][d.column("element")];
        const auto at = index.find(family);
        if (at == index.end())
            return wrong(d.name, d.row_lines[r], "the family '" + family +
                "' is no row of the table \"Families\"");
        if (element.empty())
            return wrong(d.name, d.row_lines[r], "the row names no element");
        out.families[at->second].exact_decimals.push_back(element);
    }

    const auto& e = *data.find("Schema errata");
    for (std::size_t r = 0; r < e.rows.size(); ++r) {
        const auto& written = e.rows[r][e.column("written")];
        const auto& read = e.rows[r][e.column("read")];
        if (written.empty() || read.empty())
            return wrong(e.name, e.row_lines[r], "the row lacks the name as "
                "written or as read");
        if (std::ranges::find(out.errata, written,
                &std::pair<std::string, std::string>::first) !=
            out.errata.end())
            return wrong(e.name, e.row_lines[r], "the name '" + written +
                "' is corrected twice");
        out.errata.emplace_back(written, read);
    }

    // A cell loses its leading blanks, so the table holds each line's text
    // and the comment's /* and * are written here.
    const auto& l = *data.find("Licence");
    if (l.rows.empty())
        return wrong(l.name, l.line, "the table has no line");
    for (std::size_t r = 0; r < l.rows.size(); ++r) {
        const auto& text = l.rows[r][l.column("line")];
        out.licence += r == 0 ? "/* " + text
            : text.empty() ? std::string(" *") : " * " + text;
        out.licence.push_back('\n');
    }
    return out;
}

void run(const ct::corpus_view& v, ct::file_output& out) {
    const auto data = cops::tables_for(codec_name, v.option(cops::data_option),
        needed);
    if (!data) {
        co::report(out.refusals, check_document, 0, unreadable, data.error(),
            "Give the tables the message names.");
        return;
    }
    const auto document = read_document(*data);
    if (!document) {
        co::report(out.refusals, data->source, 0, unreadable,
            document.error(), "Correct the row the message names.");
        return;
    }
    const auto& paths = document->paths;
    const xb::project project{.licence = document->licence,
        .engine_include = paths.engine_include,
        .decimal_include = paths.decimal_include,
        .decimal_type = paths.decimal_type,
        .export_macro = paths.export_macro, .errata = document->errata};
    for (const auto& row : document->families) {
        const auto header = under(paths.header_directory, "/",
            row.stem + ".hpp");
        const auto source = under(paths.source_directory, "/",
            row.stem + ".cpp");
        std::vector<std::string> texts;
        std::string missing;
        for (const auto& name : row.schemas) {
            const auto path = under(paths.schema_directory, "/", name);
            auto text = v.read_file(path);
            if (!text) {
                missing = path;
                break;
            }
            texts.push_back(std::move(*text));
        }
        if (!missing.empty()) {
            co::refuse(out, header, "cannot read the schema " + missing +
                    " of the " + row.stem + " family",
                "Restore the schema, or correct the family's schemas in " +
                    data->source + ".");
            continue;
        }
        const auto accessor = row.stem + "_document";
        const auto name_space = under(paths.namespace_prefix, "::", row.stem);
        const auto include = under(paths.include_prefix, "/",
            row.stem + ".hpp");
        const std::vector<std::string_view> exact(row.exact_decimals.begin(),
            row.exact_decimals.end());
        try {
            auto files = xb::generate(texts, {.root_element = row.root_element,
                .root_type = row.root_type, .accessor = accessor,
                .name_space = name_space, .include = include,
                .schema_names = row.schemas, .exact_decimals = exact},
                project);
            out.files.add(header, std::move(files.header));
            out.files.add(source, std::move(files.source));
        } catch (const xb::error& e) {
            co::refuse(out, header, "the schemas of the " + row.stem +
                    " family do not generate: " + e.what(),
                "Correct the schema the reason names, or the family's row in " +
                    data->source + ".");
        }
    }
}

constexpr std::array<ct::option, 3> options{{
    {"clang_format", "clang-format",
        "The clang-format the generator runs over the records and tables, a "
        "program on PATH or a path under the checkout; empty for none."},
    {"clang_format_style", ".clang-format",
        "The style file under the checkout clang-format reads."},
    {cops::data_option, "", "The tables of the check document whose "
        "Definition names this codec: the document families, the elements "
        "carried as exact decimals, the schema errata, and the settings "
        "that place the files and name the project's engine."}}};

std::vector<ct::formatter> formatters_of(const ct::option_values& o,
    const std::filesystem::path& checkout) {
    const auto name = o["clang_format"];
    if (name.empty())
        return {};
    const auto program = name.find('/') == std::string_view::npos
        ? compass::platform::process::find_program(name)
        : compass::platform::process::find_program(
            (checkout / name).generic_string());
    if (!program)
        throw std::runtime_error("xml_bindings: " + std::string(name) +
            " is not a program, for the option clang_format");
    return {{.name = "clang-format", .program = *program,
        .extensions = {".cpp", ".hpp"},
        .arguments = {"-i", "--style=file:{style}"}, .passes = 1,
        .style = checkout / std::string(o["clang_format_style"])}};
}

}

const ct::descriptor xml_bindings{
    .name = "xml_bindings",
    .version = 2,
    .summary = "Writes the C++ records and codec tables of each XML document "
               "family a project's check document lists, from its schemas.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{run},
    .formatters = formatters_of};

}
