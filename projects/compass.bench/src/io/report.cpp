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
#include "compass.bench/io/report.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <numeric>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include "compass.org/ops/type_lookup.hpp"

namespace compass::bench::io {

namespace {

constexpr std::size_t top_extensions = 15;
constexpr std::size_t top_unresolved = 10;
constexpr std::size_t top_fan_in = 15;
constexpr std::size_t top_parse = 10;
constexpr std::size_t top_untyped = 20;
constexpr std::size_t files_per_type = 5;

double median(std::vector<double> v) {
    std::ranges::sort(v);
    const auto n = v.size();
    return n % 2 == 1 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}

void write_stage_header(std::ostream& os) {
    os << std::format("{:<8}{:>10}{:>16}{:>8}{:>12}{:>10}{:>10}{:>10}\n",
        "stage", "files", "bytes", "errors", "median_ms", "min_ms", "max_ms",
        "MB/s");
}

void write_stage_row(std::ostream& os, const types::stage_result& stage) {
    const auto [min, max] = std::ranges::minmax(stage.wall_ms);
    const double med = median(stage.wall_ms);
    const double mb_per_s = med > 0 ? (stage.bytes / 1e6) / (med / 1e3) : 0;
    os << std::format(
        "{:<8}{:>10}{:>16}{:>8}{:>12.2f}{:>10.2f}{:>10.2f}{:>10.1f}\n",
        stage.name, stage.files, stage.bytes, stage.errors, med, min, max,
        mb_per_s);
}

void write_stage(std::ostream& os, const types::stage_result& stage) {
    write_stage_header(os);
    write_stage_row(os, stage);
    os << std::format("repetitions: {}  checksum: {:#018x}\n",
        stage.wall_ms.size(), stage.checksum);
}

void write_extensions(std::ostream& os, const types::file_table& table) {
    const auto n = table.extensions.size();
    std::vector<std::uint64_t> files(n), bytes(n);
    for (std::size_t i = 0; i < table.size(); ++i) {
        ++files[table.extension_ids[i]];
        bytes[table.extension_ids[i]] += table.sizes[i];
    }

    std::vector<std::uint32_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::ranges::sort(order, [&](auto a, auto b) {
        return bytes[a] != bytes[b] ? bytes[a] > bytes[b] : files[a] > files[b];
    });
    order.resize(std::min(n, top_extensions));

    os << std::format("{:<16}{:>10}{:>16}\n", "extension", "files", "bytes");
    for (const auto id : order) {
        const auto& name = table.extensions[id];
        os << std::format("{:<16}{:>10}{:>16}\n",
            name.empty() ? "(none)" : name, files[id], bytes[id]);
    }
}

void write_step(std::ostream& os, std::string_view name,
    const std::vector<double>& wall_ms) {
    const auto [min, max] = std::ranges::minmax(wall_ms);
    os << std::format("{:<10}{:>12.2f}{:>10.2f}{:>10.2f}\n", name,
        median(wall_ms), min, max);
}

void write_unresolved(std::ostream& os, const types::includes_result& stage) {
    const auto& includes = stage.includes;
    std::unordered_map<std::string, std::uint64_t> counts;
    for (std::size_t i = 0; i < includes.size(); ++i) {
        if (includes.targets[i] != types::unresolved_target)
            continue;
        const auto quoted = includes.kinds[i] == types::include_kind::quoted;
        const auto name = includes.name(i);
        ++counts[quoted ? std::format("\"{}\"", name)
                        : std::format("<{}>", name)];
    }
    std::vector<std::pair<std::string, std::uint64_t>> rows(counts.begin(),
        counts.end());
    std::ranges::sort(rows, [](const auto& a, const auto& b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    rows.resize(std::min(rows.size(), top_unresolved));

    os << std::format("top {} unresolved names\n", top_unresolved);
    os << std::format("{:>10}  {}\n", "includes", "name");
    for (const auto& [name, count] : rows)
        os << std::format("{:>10}  {}\n", count, name);
}

void write_fan_in(std::ostream& os, std::string_view title,
    std::span<const std::uint32_t> fan_in, const types::file_table& table) {
    std::vector<std::uint32_t> order(fan_in.size());
    std::iota(order.begin(), order.end(), 0);
    const auto n = std::min(order.size(), top_fan_in);
    std::ranges::partial_sort(order, order.begin() + n,
        [&](auto a, auto b) {
            return fan_in[a] != fan_in[b] ? fan_in[a] > fan_in[b] : a < b;
        });

    os << title << "\n";
    os << std::format("{:>10}  {}\n", "fan_in", "file");
    for (std::size_t i = 0; i < n && fan_in[order[i]] > 0; ++i)
        os << std::format("{:>10}  {}\n", fan_in[order[i]],
            table.paths[order[i]]);
}

template <typename Key>
std::vector<std::size_t> top_rows(std::span<const Key> key) {
    std::vector<std::size_t> order(key.size());
    std::iota(order.begin(), order.end(), 0);
    const auto n = std::min(order.size(), top_parse);
    std::ranges::partial_sort(order, order.begin() + n,
        [&](auto a, auto b) {
            return key[a] != key[b] ? key[a] > key[b] : a < b;
        });
    order.resize(n);
    return order;
}

void write_language(std::ostream& os, const types::language_result& r,
    const types::file_table& table) {
    const auto& name = r.stage.name;
    std::uint64_t files_with_errors = 0, error_nodes = 0;
    for (const auto n : r.error_nodes) {
        files_with_errors += n != 0;
        error_nodes += n;
    }
    os << std::format("{}: files with parse errors: {} of {}  "
                      "error nodes: {}\n",
        name, files_with_errors, r.ids.size(), error_nodes);

    os << std::format("top {} {} files by error nodes\n", top_parse, name);
    os << std::format("{:>10}{:>12}  {}\n", "nodes", "parse_ms", "file");
    for (const auto row : top_rows<std::uint32_t>(r.error_nodes)) {
        if (r.error_nodes[row] == 0)
            break;
        os << std::format("{:>10}{:>12.2f}  {}\n", r.error_nodes[row],
            r.parse_ms[row], table.paths[r.ids[row]]);
    }

    os << std::format("top {} {} files by parse time\n", top_parse, name);
    os << std::format("{:>10}{:>12}  {}\n", "parse_ms", "nodes", "file");
    for (const auto row : top_rows<double>(r.parse_ms))
        os << std::format("{:>10.2f}{:>12}  {}\n", r.parse_ms[row],
            r.error_nodes[row], table.paths[r.ids[row]]);
}

}

bool failed(const types::stage_result& stage) {
    return stage.files == 0 || stage.errors != 0;
}

void write_report(std::ostream& os, const types::stage_result& stage,
    const setup_timing& setup, const types::file_table& table) {
    write_stage(os, stage);
    os << std::format("threads: {}\n", setup.threads);
    os << std::format("git ls-files: {:.2f} ms\n", setup.listing_ms);
    os << std::format("file table (lstat): {:.2f} ms\n", setup.table_ms);
    os << "\n";
    write_extensions(os, table);

    if (stage.files == 0)
        os << std::format("\nFAILED: stage '{}' read zero files.\n",
            stage.name);
    if (stage.errors != 0)
        os << std::format("\nFAILED: stage '{}' hit {} error(s). First: {}\n",
            stage.name, stage.errors, stage.first_error);
}

bool failed(const types::includes_result& stage) {
    return stage.files == 0 || stage.errors != 0;
}

void write_includes_report(std::ostream& os,
    const types::includes_result& stage, const includes_setup& setup,
    const types::file_table& table) {
    os << std::format("{:<10}{:>12}{:>10}{:>10}\n", "step", "median_ms",
        "min_ms", "max_ms");
    write_step(os, "scan", stage.scan_ms);
    write_step(os, "resolve", stage.resolve_ms);
    write_step(os, "graph", stage.graph_ms);
    write_step(os, "closure", stage.closure_ms);
    os << std::format("repetitions: {}  scan threads: {}\n",
        stage.scan_ms.size(), setup.common.threads);

    const auto& includes = stage.includes;
    std::uint64_t quoted = 0, internal = 0, external = 0, unresolved = 0;
    for (std::size_t i = 0; i < includes.size(); ++i) {
        quoted += includes.kinds[i] == types::include_kind::quoted;
        const auto t = includes.targets[i];
        if (t == types::external_target)
            ++external;
        else if (t == types::unresolved_target)
            ++unresolved;
        else
            ++internal;
    }
    os << std::format("files scanned: {}  bytes: {}  errors: {}\n",
        stage.files, stage.bytes, stage.errors);
    os << std::format("includes found: {} (quoted {}, angle {}); "
                      "macro operands skipped: {}\n",
        includes.size(), quoted, includes.size() - quoted, includes.computed);
    os << std::format("in checkout: {}  external: {}  unresolved: {}\n",
        internal, external, unresolved);
    os << std::format("edges: {} (distinct includer and target pairs)  "
                      "source files: {}\n",
        stage.graph.targets.size(), stage.roots.size());
    os << std::format("git ls-files: {:.2f} ms  file table (lstat): {:.2f} ms"
                      "  compile commands: {:.2f} ms\n",
        setup.common.listing_ms, setup.common.table_ms,
        setup.compile_commands_ms);
    os << std::format("compile commands: {}  directories: {}  contexts: {}  "
                      "implicit system directories: {}\n",
        setup.commands, setup.directories, setup.contexts,
        setup.implicit_system);

    os << "\n";
    write_unresolved(os, stage);
    os << "\n";
    write_fan_in(os, std::format("top {} by direct fan-in", top_fan_in),
        stage.direct_fan_in, table);
    os << "\n";
    write_fan_in(os,
        std::format("top {} by transitive fan-in (source files reaching it)",
            top_fan_in),
        stage.transitive_fan_in, table);

    if (stage.files == 0)
        os << "\nFAILED: stage 'includes' scanned zero files.\n";
    if (stage.errors != 0)
        os << std::format(
            "\nFAILED: stage 'includes' hit {} error(s). First: {}\n",
            stage.errors, stage.first_error);
}

bool failed(const types::parse_result& stage) {
    const bool none_parsed = std::ranges::all_of(stage.languages,
        [](const auto& r) { return r.stage.files == 0; });
    const bool io_errors = std::ranges::any_of(stage.languages,
        [](const auto& r) { return r.stage.errors != 0; });
    return none_parsed || io_errors;
}

void write_parse_report(std::ostream& os, const types::parse_result& stage,
    const setup_timing& setup, const types::file_table& table) {
    write_stage_header(os);
    for (const auto& r : stage.languages)
        write_stage_row(os, r.stage);
    os << std::format("repetitions: {}  threads: {}\n",
        stage.languages.front().stage.wall_ms.size(), setup.threads);
    os << std::format(
        "git ls-files: {:.2f} ms  file table (lstat): {:.2f} ms\n",
        setup.listing_ms, setup.table_ms);
    for (const auto& r : stage.languages) {
        os << "\n";
        write_language(os, r, table);
    }

    if (std::ranges::all_of(stage.languages,
            [](const auto& r) { return r.stage.files == 0; }))
        os << "\nFAILED: stage 'parse' parsed zero files.\n";
    for (const auto& r : stage.languages) {
        if (r.stage.files == 0)
            os << std::format("\nnote: no {} files in the checkout.\n",
                r.stage.name);
        if (r.stage.errors != 0)
            os << std::format(
                "\nFAILED: stage '{}' hit {} error(s). First: {}\n",
                r.stage.name, r.stage.errors, r.stage.first_error);
    }
}

bool failed(const types::org_result& stage) {
    return failed(stage.stage);
}

void write_org_report(std::ostream& os, const types::org_result& stage,
    const setup_timing& setup, const types::file_table& table) {
    write_stage_header(os);
    write_stage_row(os, stage.stage);
    os << std::format("repetitions: {}  threads: {}\n",
        stage.stage.wall_ms.size(), setup.threads);
    os << std::format(
        "git ls-files: {:.2f} ms  file table (lstat): {:.2f} ms\n",
        setup.listing_ms, setup.table_ms);
    os << std::format("rows: elements {}  objects {}  headings {}  links {}"
                      "  names {}\n",
        stage.elements, stage.objects, stage.headings, stage.links,
        stage.names);

    os << std::format("\ntop {} org files by parse time\n", top_parse);
    os << std::format("{:>10}{:>12}  {}\n", "parse_ms", "bytes", "file");
    for (const auto row : top_rows<double>(stage.parse_ms))
        os << std::format("{:>10.2f}{:>12}  {}\n", stage.parse_ms[row],
            table.sizes[stage.ids[row]], table.paths[stage.ids[row]]);

    if (stage.stage.files == 0)
        os << "\nFAILED: stage 'org' parsed zero files.\n";
    if (stage.stage.errors != 0)
        os << std::format("\nFAILED: stage 'org' hit {} error(s). First: {}\n",
            stage.stage.errors, stage.stage.first_error);
}

bool failed(const types::types_result& stage) {
    return stage.meta.paths.empty() || failed(stage.stage);
}

void write_types_report(std::ostream& os, const types::types_result& stage,
    const setup_timing& setup, const types::file_table& table) {
    namespace meta = ingest::meta;
    const auto& registry = stage.meta.registry;
    const auto type_count = registry.types.size();

    write_stage_header(os);
    write_stage_row(os, stage.stage);
    os << std::format("repetitions: {}  threads: {}\n",
        stage.stage.wall_ms.size(), setup.threads);
    os << std::format(
        "git ls-files: {:.2f} ms  file table (lstat): {:.2f} ms\n",
        setup.listing_ms, setup.table_ms);
    if (!stage.registry_ms.empty())
        os << std::format("registry build (median): {:.3f} ms\n",
            median(stage.registry_ms));
    os << std::format("resolve (summed over threads, last repetition): "
                      "{:.3f} ms\n", stage.resolve_ms);

    const auto models = std::ranges::count(registry.types.kinds.values,
        compass::org::types::type_kind::model);
    os << std::format("\nregistry: {} types ({} document, {} model) from {} "
                      "contract pages\n",
        type_count, type_count - models, models, stage.meta.paths.size());

    constexpr std::array<std::string_view, 3> level_names{
        "missing", "item", "unknown"};
    constexpr std::array<std::string_view, 3> folder_names{
        "missing", "folder", "path"};
    constexpr std::array<std::string_view, 5> state_names{
        "missing", "todo_item", "state_item", "todo_block", "not_stateful"};
    constexpr std::array<std::string_view, 3> sections_names{
        "missing", "item", "heading"};
    os << std::format("{:<6}{:>6}{:>9}{:>8}{:>14}{:>10}{:>6}  {}\n", "fits",
        "types", "level", "folder", "state", "sections", "n", "page");
    for (std::size_t i = 0; i < stage.meta.paths.size(); ++i) {
        const auto& r = stage.meta.reports[i];
        os << std::format("{:<6}{:>6}{:>9}{:>8}{:>14}{:>10}{:>6}  {}{}{}\n",
            meta::fits(r) ? "yes" : "NO", r.types,
            level_names[static_cast<std::size_t>(r.level)],
            folder_names[static_cast<std::size_t>(r.folder)],
            state_names[static_cast<std::size_t>(r.state)],
            sections_names[static_cast<std::size_t>(r.sections_from)],
            r.sections, stage.meta.paths[i],
            r.contract ? "" : " (no Contract heading)",
            r.duplicate ? " (duplicate type)" : "");
    }

    std::vector<std::uint64_t> counts(type_count);
    std::uint64_t untyped = 0;
    std::map<std::string, std::vector<std::uint32_t>> unresolved;
    for (std::size_t row = 0; row < stage.ids.size(); ++row) {
        switch (stage.outcomes[row]) {
        case types::resolution::resolved:
            ++counts[stage.types[row].value];
            break;
        case types::resolution::unresolved:
            unresolved[stage.values[row]].push_back(stage.ids[row]);
            break;
        case types::resolution::untyped:
            ++untyped;
            break;
        }
    }

    os << "\ndocuments per type\n";
    os << std::format("{:>10}{:>10}  {}\n", "documents", "kind", "type");
    std::vector<std::uint32_t> order(type_count);
    std::iota(order.begin(), order.end(), 0);
    std::ranges::stable_sort(order, std::greater{},
        [&](std::uint32_t t) { return counts[t]; });
    std::string unused;
    for (const auto t : order) {
        const compass::org::types::type_id id{t};
        const auto name = compass::org::ops::name_text(registry,
            registry.types.names[id]);
        if (counts[t] == 0) {
            unused += unused.empty() ? "" : " ";
            unused += name;
            continue;
        }
        os << std::format("{:>10}{:>10}  {}\n", counts[t],
            registry.types.kinds[id] == compass::org::types::type_kind::model
                ? "model" : "document",
            name);
    }
    os << std::format("registered with no document: {}\n",
        unused.empty() ? "none" : unused);

    os << std::format("\ndocuments with no #+type: {}\n", untyped);
    std::size_t shown = 0;
    for (std::size_t row = 0;
         row < stage.ids.size() && shown < top_untyped; ++row) {
        if (stage.outcomes[row] != types::resolution::untyped)
            continue;
        os << "  " << table.paths[stage.ids[row]] << "\n";
        ++shown;
    }
    if (untyped > shown)
        os << std::format("  ... and {} more\n", untyped - shown);

    os << std::format("\nunresolved types: {}\n", unresolved.size());
    std::vector<std::pair<std::string, std::vector<std::uint32_t>>> rows(
        unresolved.begin(), unresolved.end());
    std::ranges::stable_sort(rows, std::greater{},
        [](const auto& r) { return r.second.size(); });
    for (const auto& [value, files] : rows) {
        os << std::format("{:>10}  {}\n", files.size(), value);
        for (std::size_t i = 0; i < std::min(files.size(), files_per_type);
             ++i)
            os << "            " << table.paths[files[i]] << "\n";
    }

    if (stage.meta.paths.empty())
        os << "\nFAILED: stage 'types' found no document_type_*.org page.\n";
    if (stage.stage.files == 0)
        os << "\nFAILED: stage 'types' parsed zero files.\n";
    if (stage.stage.errors != 0)
        os << std::format(
            "\nFAILED: stage 'types' hit {} error(s). First: {}\n",
            stage.stage.errors, stage.stage.first_error);
}

bool failed(const types::store_result& stage) {
    return failed(stage.cold) || stage.rejected != 0 ||
        stage.mismatches != 0;
}

void write_store_report(std::ostream& os, const types::store_result& stage,
    const setup_timing& setup, const std::filesystem::path& objects) {
    write_stage_header(os);
    write_stage_row(os, stage.cold);
    os << std::format("repetitions: {}  threads: {}\n",
        stage.cold.wall_ms.size(), setup.threads);
    os << std::format(
        "git ls-files: {:.2f} ms  file table (lstat): {:.2f} ms\n",
        setup.listing_ms, setup.table_ms);
    os << std::format("store: {}\n", objects.string());

    os << std::format("\n{:<14}{:>12}{:>10}{:>10}\n", "run", "median_ms",
        "min_ms", "max_ms");
    const auto run = [&](std::string_view name,
        const std::vector<double>& wall_ms) {
        if (wall_ms.empty())
            return;
        const auto [min, max] = std::ranges::minmax(wall_ms);
        os << std::format("{:<14}{:>12.2f}{:>10.2f}{:>10.2f}\n", name,
            median(wall_ms), min, max);
    };
    run("cold", stage.cold.wall_ms);
    run("warm map", stage.map_ms);
    run("warm verify", stage.verify_ms);

    const double ratio = stage.cold.bytes > 0
        ? static_cast<double>(stage.object_bytes) / stage.cold.bytes : 0;
    os << std::format("\nobjects: {} for {} files\n", stage.objects,
        stage.cold.files);
    os << std::format("object bytes: {}  source bytes: {}  ratio: {:.3f}\n",
        stage.object_bytes, stage.cold.bytes, ratio);
    os << std::format("rejected objects: {}  dump mismatches: {}\n",
        stage.rejected, stage.mismatches);

    if (stage.cold.files == 0)
        os << "\nFAILED: stage 'store' stored zero files.\n";
    if (stage.cold.errors != 0)
        os << std::format(
            "\nFAILED: stage 'store' hit {} error(s). First: {}\n",
            stage.cold.errors, stage.cold.first_error);
    if (stage.rejected != 0)
        os << std::format("\nFAILED: {} object(s) the stage wrote did not "
                          "load.\n", stage.rejected);
    if (stage.mismatches != 0)
        os << std::format("\nFAILED: {} loaded object(s) dump differently "
                          "from their parse. First: {}\n",
            stage.mismatches, stage.first_mismatch);
}

bool failed(const types::ids_result& stage) {
    return stage.files == 0 || stage.errors != 0 || stage.mismatches != 0;
}

void write_ids_report(std::ostream& os, const types::ids_result& stage,
    const setup_timing& setup) {
    os << std::format("index: version {}  entries: {}  trusted: {}  "
                      "autocrlf: {}\n", stage.index_version,
        stage.index_entries, stage.trusted_entries,
        stage.autocrlf ? "converts" : "no");
    os << std::format("repetitions: {}  threads: {}\n", stage.read_ms.size(),
        setup.threads);
    os << std::format(
        "git ls-files: {:.2f} ms  file table (lstat): {:.2f} ms\n",
        setup.listing_ms, setup.table_ms);

    const auto share = [&](std::uint64_t n) {
        return stage.files > 0 ? 100.0 * n / stage.files : 0;
    };
    os << std::format("\n{:<8}{:>10}{:>9}{:>16}\n", "source", "files", "%",
        "bytes");
    os << std::format("{:<8}{:>10}{:>9.2f}{:>16}\n", "index",
        stage.from_index, share(stage.from_index), "");
    os << std::format("{:<8}{:>10}{:>9.2f}{:>16}\n", "hashed", stage.hashed,
        share(stage.hashed), stage.hashed_bytes);
    os << std::format("{:<8}{:>10}{:>9}{:>16}\n", "errors", stage.errors, "",
        "");
    os << std::format("{:<8}{:>10}\n", "total", stage.files);

    os << std::format("\n{:<14}{:>12}{:>10}{:>10}\n", "step", "median_ms",
        "min_ms", "max_ms");
    const auto step = [&](std::string_view name,
        const std::vector<double>& wall_ms) {
        if (wall_ms.empty())
            return;
        const auto [min, max] = std::ranges::minmax(wall_ms);
        os << std::format("{:<14}{:>12.2f}{:>10.2f}{:>10.2f}\n", name,
            median(wall_ms), min, max);
    };
    step("read index", stage.read_ms);
    step("stat", stage.stat_ms);
    step("hash", stage.hash_ms);
    std::vector<double> total;
    for (std::size_t i = 0; i < stage.read_ms.size(); ++i)
        total.push_back(stage.read_ms[i] + stage.stat_ms[i] +
            stage.hash_ms[i]);
    step("total", total);

    os << std::format("\nverified against git hash-object: {}  "
                      "mismatches: {}\n", stage.verified, stage.mismatches);

    if (stage.files == 0)
        os << "\nFAILED: stage 'ids' resolved zero files.\n";
    if (stage.errors != 0)
        os << std::format(
            "\nFAILED: stage 'ids' could not hash {} file(s). First: {}\n",
            stage.errors, stage.first_error);
    if (stage.mismatches != 0)
        os << std::format("\nFAILED: {} ID(s) differ from git hash-object. "
                          "First: {}\n", stage.mismatches,
            stage.first_mismatch);
}

}
