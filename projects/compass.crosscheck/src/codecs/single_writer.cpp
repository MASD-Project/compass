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
#include "compass.crosscheck/codecs/single_writer.hpp"

#include <array>
#include <cctype>
#include <map>
#include <set>
#include <span>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::array<std::string_view, 1> collapse_the_double_writers{
    "A611C520-54AA-4562-9BA6-768278E5DDEE"};

constexpr std::array<ct::rule, 1> rules{{
    {"second_writer", "No table a DQ publish function writes is also "
        "written by a populate script, unless the two write different "
        "tenants or different rows.", collapse_the_double_writers,
        ct::blame_method::line}}};

constexpr std::string_view create_root = "projects/ores.sql/create/";
constexpr std::string_view populate_root = "projects/ores.sql/populate/";
constexpr std::string_view publish_suffix = "_publish_from_dq_create.sql";

constexpr std::uint32_t publish_file = 0;
constexpr std::uint32_t populate_file = 1;

const pt::regex& insert_into() {
    static const pt::regex out(R"re(insert\s+into\s+"?(\w+)"?)re",
        {.ignorecase = true});
    return out;
}

/**
 * @brief @p sql without its comments, block comments removed first and
 * line comments after, as the script removed them, with the offset in
 * @p sql of each byte kept.
 */
struct uncommented {
    std::string text;
    std::vector<std::uint32_t> offsets;
};

uncommented without(std::string_view sql, std::string_view open,
    std::string_view close, const std::vector<std::uint32_t>& offsets) {
    uncommented out;
    std::size_t at = 0;
    while (at < sql.size()) {
        const auto start = sql.find(open, at);
        auto end = start == std::string_view::npos
            ? std::string_view::npos : sql.find(close, start + open.size());
        // A line comment runs to the end of the text when no newline ends it.
        if (start != std::string_view::npos && end == std::string_view::npos
            && close == "\n")
            end = sql.size();
        if (end == std::string_view::npos) {
            for (; at < sql.size(); ++at) {
                out.text.push_back(sql[at]);
                out.offsets.push_back(offsets[at]);
            }
            break;
        }
        for (; at < start; ++at) {
            out.text.push_back(sql[at]);
            out.offsets.push_back(offsets[at]);
        }
        at = close == "\n" ? end : end + close.size();
    }
    return out;
}

uncommented uncomment(std::string_view sql) {
    std::vector<std::uint32_t> offsets(sql.size());
    for (std::uint32_t i = 0; i < offsets.size(); ++i)
        offsets[i] = i;
    const auto blocks = without(sql, "/*", "*/", offsets);
    return without(blocks.text, "--", "\n", blocks.offsets);
}

std::string lowered(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    auto out = v.list_files(create_root, publish_suffix);
    const auto populate = v.list_files(populate_root, ".sql");
    out.insert(out.end(), populate.begin(), populate.end());
    return out;
}

/**
 * @brief The fact of a file: whether it is a publish or a populate file,
 * then each table it inserts into with the offset of the line of its first
 * insert into it.
 */
void map(std::string_view path, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    const auto sql = uncomment(bytes);
    std::map<std::string, std::uint32_t> first;
    for (const auto& m : insert_into().find_all(sql.text)) {
        const auto offset = sql.offsets[m.start()];
        first.try_emplace(lowered(m.group(1)), co::offset_of_line(bytes,
            co::line_of(bytes, offset)));
    }
    ct::fact_writer fact(out.fact);
    fact.number(path.starts_with(populate_root) ? populate_file
        : publish_file);
    for (const auto& [table, offset] : first)
        fact.text(table).number(offset);
}

void reduce(std::span<const ct::scanned_file> files, const ct::corpus_view&,
    ct::diagnostic_table& out) {
    std::set<std::string, std::less<>> published;
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        if (fact.number() != publish_file)
            continue;
        while (!fact.done()) {
            published.emplace(fact.text());
            (void)fact.number();
        }
    }
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        if (fact.number() != populate_file)
            continue;
        while (!fact.done()) {
            const std::string table(fact.text());
            const auto offset = fact.number();
            if (!published.contains(table))
                continue;
            co::report(out, f.path, offset, 0, "writes " + table +
                ", which a DQ publish function also writes",
                "Keep the writer the table's level assigns and remove the "
                "other. A populate script that writes another tenant's "
                "rows, or rows the publish function never writes, is not "
                "a second copy: excuse it with a compass:except comment "
                "that says why.");
        }
    }
}

}

const ct::descriptor single_writer{
    .name = "single_writer",
    .version = 1,
    .summary = "Reports each populate script that writes a table a DQ "
        "publish function also writes.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, reduce}};

}
