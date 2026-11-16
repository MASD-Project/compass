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
#include <algorithm>
#include <charconv>
#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "compass.bench/io/compile_commands.hpp"
#include "compass.bench/io/report.hpp"
#include "compass.bench/ops/includes_stage.hpp"
#include "compass.bench/ops/listing.hpp"
#include "compass.bench/ops/org_dump.hpp"
#include "compass.bench/ops/org_stage.hpp"
#include "compass.bench/ops/parse_stage.hpp"
#include "compass.bench/ops/partition.hpp"
#include "compass.bench/ops/read_stage.hpp"
#include "compass.bench/ops/search_paths.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"

namespace {

using namespace compass::bench;

constexpr std::string_view usage =
    "usage: compass-bench read <checkout> [-j N] [--repeat R]\n"
    "       compass-bench includes <checkout> --compile-commands <file>\n"
    "           --source-root <dir> [--isystem <dir>]... [-j N] [--repeat R]\n"
    "       compass-bench parse <checkout> [-j N] [--repeat R]\n"
    "       compass-bench org <checkout> [-j N] [--repeat R]\n"
    "       compass-bench org-dump <file>\n"
    "       compass-bench org-dump --list <list>\n";

enum class stage { read, includes, parse, org, org_dump };

struct options {
    stage which = stage::read;
    std::filesystem::path checkout;
    unsigned threads = 0;
    unsigned repetitions = 5;
    std::filesystem::path compile_commands;
    std::filesystem::path source_root;
    std::vector<std::string> implicit_system;
    std::filesystem::path dump_file;
    std::filesystem::path dump_list;
};

std::filesystem::path absolute_path(const std::filesystem::path& p) {
    auto r = std::filesystem::absolute(p).lexically_normal();
    if (!r.has_filename() && r.has_relative_path())
        r = r.parent_path();
    return r;
}

std::optional<unsigned> parse_positive(std::string_view s) {
    unsigned v = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || end != s.data() + s.size() || v == 0)
        return std::nullopt;
    return v;
}

std::optional<options> parse_dump_options(int argc, char** argv) {
    options o;
    o.which = stage::org_dump;
    if (argc == 3 && std::string_view(argv[2]) != "--list")
        o.dump_file = argv[2];
    else if (argc == 4 && std::string_view(argv[2]) == "--list")
        o.dump_list = argv[3];
    else
        return std::nullopt;
    return o;
}

std::optional<options> parse_options(int argc, char** argv) {
    if (argc < 3)
        return std::nullopt;
    options o;
    const std::string_view name = argv[1];
    if (name == "read")
        o.which = stage::read;
    else if (name == "includes")
        o.which = stage::includes;
    else if (name == "parse")
        o.which = stage::parse;
    else if (name == "org")
        o.which = stage::org;
    else if (name == "org-dump")
        return parse_dump_options(argc, argv);
    else
        return std::nullopt;
    o.checkout = argv[2];
    o.threads = std::max(1u, std::thread::hardware_concurrency());
    const bool includes = o.which == stage::includes;
    for (int i = 3; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (i + 1 >= argc)
            return std::nullopt;
        const std::string_view value = argv[++i];
        if (flag == "-j" || flag == "--repeat") {
            const auto n = parse_positive(value);
            if (!n)
                return std::nullopt;
            (flag == "-j" ? o.threads : o.repetitions) = *n;
        } else if (includes && flag == "--compile-commands") {
            o.compile_commands = value;
        } else if (includes && flag == "--source-root") {
            o.source_root = value;
        } else if (includes && flag == "--isystem") {
            o.implicit_system.emplace_back(value);
        } else {
            return std::nullopt;
        }
    }
    if (includes && (o.compile_commands.empty() || o.source_root.empty()))
        return std::nullopt;
    // Compile commands hold absolute paths, so every path that is compared
    // against them must be absolute too, or rebasing silently matches nothing.
    o.checkout = absolute_path(o.checkout);
    if (includes) {
        o.compile_commands = absolute_path(o.compile_commands);
        o.source_root = absolute_path(o.source_root);
        for (auto& dir : o.implicit_system)
            dir = absolute_path(dir).string();
    }
    return o;
}

double elapsed_ms(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

types::file_table list_checkout(const options& o, io::setup_timing& setup) {
    setup.threads = o.threads;
    auto start = std::chrono::steady_clock::now();
    const auto raw = ops::git_ls_files(o.checkout);
    const auto entries = ops::parse_listing(raw);
    setup.listing_ms = elapsed_ms(start);

    start = std::chrono::steady_clock::now();
    auto table = ops::build_file_table(o.checkout, entries);
    setup.table_ms = elapsed_ms(start);
    return table;
}

int run_read(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto ranges = ops::balance_ranges(table.sizes, o.threads);
    const auto stage =
        ops::run_read_stage(o.checkout, table, ranges, o.repetitions);

    io::write_report(std::cout, stage, setup, table);
    return io::failed(stage) ? 1 : 0;
}

int run_includes(const options& o) {
    io::includes_setup setup;
    const auto table = list_checkout(o, setup.common);

    const auto start = std::chrono::steady_clock::now();
    const compass::platform::filesystem::mapped_file database(
        o.compile_commands);
    const auto commands = io::parse_compile_commands(database.text());
    const auto paths = ops::build_search_paths(commands, o.source_root,
        o.implicit_system, table);
    setup.compile_commands_ms = elapsed_ms(start);
    setup.commands = commands.size();
    setup.directories = paths.directories.size();
    setup.contexts = paths.contexts.size();
    setup.implicit_system = o.implicit_system.size();

    const auto stage = ops::run_includes_stage(o.checkout, table, paths,
        o.threads, o.repetitions);
    io::write_includes_report(std::cout, stage, setup, table);
    return io::failed(stage) ? 1 : 0;
}

int run_parse(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto stage =
        ops::run_parse_stage(o.checkout, table, o.threads, o.repetitions);
    io::write_parse_report(std::cout, stage, setup, table);
    return io::failed(stage) ? 1 : 0;
}

int run_org(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto stage =
        ops::run_org_stage(o.checkout, table, o.threads, o.repetitions);
    io::write_org_report(std::cout, stage, setup, table);
    return io::failed(stage) ? 1 : 0;
}

int run_org_dump(const options& o) {
    if (o.dump_list.empty())
        ops::dump_org_file(o.dump_file, std::cout);
    else
        ops::dump_org_list(o.dump_list);
    return 0;
}

int run(const options& o) {
    switch (o.which) {
    case stage::read: return run_read(o);
    case stage::includes: return run_includes(o);
    case stage::parse: return run_parse(o);
    case stage::org: return run_org(o);
    case stage::org_dump: return run_org_dump(o);
    }
    return 2;
}

}

int main(int argc, char** argv) {
    const auto o = parse_options(argc, argv);
    if (!o) {
        std::cerr << usage;
        return 2;
    }
    try {
        return run(*o);
    } catch (const std::exception& e) {
        std::cerr << "compass-bench: " << e.what() << "\n";
        return 1;
    }
}
