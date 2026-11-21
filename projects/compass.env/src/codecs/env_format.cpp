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
#include "compass.env/codecs/env_format.hpp"

#include <array>
#include <cstdint>
#include <string>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.env/ops/format.hpp"

namespace compass::env::codecs {

namespace {

namespace ct = compass::codec::types;
namespace cops = compass::codec::ops;

constexpr std::array<ct::rule, 6> rules{{
    {"unknown_version", "Every variable of the environment file's check "
        "document names the version that added it, and any that removed "
        "it, and the Versions table has each."},
    {"undeclared", "A version after the baseline lists every variable whose "
        "row says the version added or removed it, so a variable is never "
        "added without a new version."},
    {"unknown_variable", "Every variable a version lists as added or "
        "removed has a row that names that version."},
    {"empty_version", "Every version after the baseline adds or removes a "
        "variable, or is marked format only."},
    {"unreadable", "The check document gives every table, column, setting, "
        "kind and template the format needs, and each row reads."},
    {"duplicate_variable", "Two live rows of one variable each have a "
        "when, so that exclusive conditions write it once; a row without a "
        "when is the variable's only row for the services it applies to."}}};

/**
 * @brief The byte offset of line @p line, from 1, of @p text; 0 for line
 * 0 or a line past its end.
 */
std::uint32_t offset_of(std::string_view text, std::uint32_t line) {
    std::size_t at = 0;
    for (std::uint32_t l = 1; l < line; ++l) {
        const auto nl = text.find('\n', at);
        if (nl == std::string_view::npos)
            return 0;
        at = nl + 1;
    }
    return static_cast<std::uint32_t>(at);
}

void report(ct::diagnostic_table& out, std::string_view file,
    std::uint32_t offset, const types::finding& f) {
    out.add({.document = compass::org::types::document_id::none(),
        .range = {offset, offset},
        .rule = ct::rule_id{f.rule},
        .level = ct::severity::error,
        .message = f.message,
        .fix = f.fix,
        .file = file});
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto text = v.option(cops::data_option);
    const auto tables = text.empty()
        ? cops::tables_for(ops::codec_name, text, {})
        : cops::decode_data_tables(text);
    if (!tables) {
        report(out, "compass/checks/env_format.org", 0,
            {.rule = ops::unreadable, .message = tables.error(),
                .fix = "Give the tables the message names."});
        return;
    }
    const auto source = v.read_file(tables->source).value_or(std::string{});
    const auto format = ops::read_format(*tables);
    if (!format) {
        report(out, tables->source, offset_of(source, format.error().line),
            format.error());
        return;
    }
    for (const auto& f : ops::check_format(*format))
        report(out, tables->source, offset_of(source, f.line), f);
}

constexpr std::array<ct::option, 1> options{{
    {cops::data_option, "", "The tables of the check document whose "
        "Definition names this codec: the settings, versions, variables, "
        "comments and inputs of the project's environment file. compass "
        "compiles them; not a setting."}}};

}

const ct::descriptor env_format{
    .name = "env_format",
    .version = 2,
    .summary = "Reports each variable of the project's environment file "
               "that no new version declares, each version that declares "
               "nothing, and each variable whose rows could write it "
               "twice.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
