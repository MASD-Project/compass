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
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>
#include "compass.bench/io/compile_commands.hpp"
#include "compass.bench/io/report.hpp"
#include "compass.bench/ops/codecs_stage.hpp"
#include "compass.bench/ops/gc_stage.hpp"
#include "compass.bench/ops/ids_stage.hpp"
#include "compass.bench/ops/includes_stage.hpp"
#include "compass.bench/ops/listing.hpp"
#include "compass.bench/ops/models_stage.hpp"
#include "compass.bench/ops/html_stage.hpp"
#include "compass.bench/ops/org_dump.hpp"
#include "compass.bench/ops/org_stage.hpp"
#include "compass.bench/ops/parse_stage.hpp"
#include "compass.bench/ops/partition.hpp"
#include "compass.bench/ops/read_stage.hpp"
#include "compass.bench/ops/render_stage.hpp"
#include "compass.bench/ops/search_paths.hpp"
#include "compass.bench/ops/search_stage.hpp"
#include "compass.bench/ops/store_stage.hpp"
#include "compass.bench/ops/types_stage.hpp"
#include "compass.bench/ops/warm_stage.hpp"
#include "compass.cli/app/codecs.hpp"
#include "compass.cli/config/precedence.hpp"
#include "compass.cli/config/repository_file.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codegen/codecs/codegen.hpp"
#include "compass.db/types/search.hpp"
#include "compass.platform/filesystem/mapped_file.hpp"
#include "compass.platform/process/standard_streams.hpp"
#include "compass.store/io/location.hpp"

namespace {

using namespace compass::bench;

constexpr std::string_view usage =
    "usage: compass-bench read <checkout> [-j N] [--repeat R]\n"
    "       compass-bench includes <checkout> --compile-commands <file>\n"
    "           --source-root <dir> [--isystem <dir>]... [-j N] [--repeat R]\n"
    "       compass-bench parse <checkout> [-j N] [--repeat R]\n"
    "       compass-bench org <checkout> [-j N] [--repeat R]\n"
    "       compass-bench types <checkout> --meta <dir> [-j N] [--repeat R]\n"
    "       compass-bench store <checkout> [--store <dir>] [-j N]\n"
    "           [--repeat R]\n"
    "       compass-bench warm <checkout> [--store <dir>] [-j N]\n"
    "           [--repeat R]\n"
    "       compass-bench codecs <checkout> --meta <dir> [--store <dir>]\n"
    "           [--set <codec>.<key>=<value>]...\n"
    "           [--write <root> | --diff <root>] [-j N] [--repeat R]\n"
    "       compass-bench gc <checkout> [--store <dir>] [--cap <bytes>]\n"
    "           [--grace <seconds>] [--dry-run]\n"
    "       compass-bench ids <checkout> [-j N] [--repeat R]\n"
    "           [--verify N|all]\n"
    "       compass-bench models <checkout> [--dump <dir>] [--repeat R]\n"
    "       compass-bench render <checkout> [--oracle <dir>] [--out <dir>]\n"
    "           [--format <git-dir>] [-j N] [--repeat R]\n"
    "       compass-bench html <root> [--out <dir>] [--repeat R]\n"
    "       compass-bench search <conninfo> --environment <name>\n"
    "           --questions <file> [--weights D,C,B,A] [--normalization N]\n"
    "           [--type-order <type>,...] [--repeat R]\n"
    "       compass-bench org-dump <file>\n"
    "       compass-bench org-dump --list <list>\n";

enum class stage {
    read, includes, parse, org, types, store, warm, codecs, gc, ids,
    models, render, html, search, org_dump
};

struct options {
    stage which = stage::read;
    std::filesystem::path checkout;
    unsigned threads = 0;
    unsigned repetitions = 5;
    std::filesystem::path compile_commands;
    std::filesystem::path source_root;
    std::vector<std::string> implicit_system;
    std::filesystem::path meta;
    std::filesystem::path store;
    std::filesystem::path output_root;
    std::filesystem::path diff_root;
    compass::codec::types::configuration settings;
    compass::store::types::collection_options collection;
    std::uint64_t verify = 0;
    std::filesystem::path dump_file;
    std::filesystem::path dump_list;
    std::filesystem::path model_dump;
    std::filesystem::path oracle;
    std::filesystem::path render_out;
    std::filesystem::path format_store;
    std::string conninfo;
    std::string environment;
    std::filesystem::path questions;
    compass::db::types::ranking ranking;
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

std::optional<std::uint64_t> parse_count(std::string_view s) {
    std::uint64_t v = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || end != s.data() + s.size())
        return std::nullopt;
    return v;
}

/**
 * @brief Adds the setting written as <codec>.<key>=<value>; false when
 * @p s is not of that form.
 */
bool add_setting(std::string_view s,
    compass::codec::types::configuration& settings) {
    const auto equals = s.find('=');
    const auto dot = s.find('.');
    if (equals == std::string_view::npos || dot == 0 || dot + 1 >= equals)
        return false;
    settings.set(s.substr(0, dot), s.substr(dot + 1, equals - dot - 1),
        s.substr(equals + 1));
    return true;
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

std::optional<options> parse_search_options(int argc, char** argv) {
    options o;
    o.which = stage::search;
    o.conninfo = argv[2];
    for (int i = 3; i + 1 < argc; i += 2) {
        const std::string_view flag = argv[i];
        const std::string_view value = argv[i + 1];
        if (flag == "--environment") {
            o.environment = value;
        } else if (flag == "--questions") {
            o.questions = value;
        } else if (flag == "--repeat") {
            const auto n = parse_positive(value);
            if (!n)
                return std::nullopt;
            o.repetitions = *n;
        } else if (flag == "--normalization") {
            const auto n = parse_count(value);
            if (!n)
                return std::nullopt;
            o.ranking.normalization = static_cast<std::uint32_t>(*n);
        } else if (flag == "--weights") {
            std::istringstream in{std::string(value)};
            char comma = 0;
            auto& w = o.ranking.weights;
            if (!(in >> w[0] >> comma >> w[1] >> comma >> w[2] >> comma >>
                    w[3]))
                return std::nullopt;
        } else if (flag == "--type-order") {
            o.ranking.type_order.clear();
            std::istringstream in{std::string(value)};
            for (std::string t; std::getline(in, t, ',');)
                o.ranking.type_order.push_back(t);
        } else {
            return std::nullopt;
        }
    }
    if ((argc - 3) % 2 != 0 || o.environment.empty() || o.questions.empty())
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
    else if (name == "types")
        o.which = stage::types;
    else if (name == "store")
        o.which = stage::store;
    else if (name == "warm")
        o.which = stage::warm;
    else if (name == "codecs")
        o.which = stage::codecs;
    else if (name == "gc")
        o.which = stage::gc;
    else if (name == "ids")
        o.which = stage::ids;
    else if (name == "models")
        o.which = stage::models;
    else if (name == "render")
        o.which = stage::render;
    else if (name == "html")
        o.which = stage::html;
    else if (name == "org-dump")
        return parse_dump_options(argc, argv);
    else if (name == "search")
        return parse_search_options(argc, argv);
    else
        return std::nullopt;
    o.checkout = argv[2];
    o.threads = std::max(1u, std::thread::hardware_concurrency());
    const bool includes = o.which == stage::includes;
    const bool codecs = o.which == stage::codecs;
    const bool typed = o.which == stage::types || codecs;
    const bool collect = o.which == stage::gc;
    const bool stored = o.which == stage::store || o.which == stage::warm ||
        collect || codecs;
    const bool ids = o.which == stage::ids;
    const bool models = o.which == stage::models;
    const bool render = o.which == stage::render;
    const bool html = o.which == stage::html;
    for (int i = 3; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (collect && flag == "--dry-run") {
            o.collection.dry_run = true;
            continue;
        }
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
        } else if (typed && flag == "--meta") {
            o.meta = value;
        } else if (stored && flag == "--store") {
            o.store = absolute_path(value);
        } else if (codecs && flag == "--write") {
            o.output_root = absolute_path(value);
        } else if (codecs && flag == "--diff") {
            o.diff_root = absolute_path(value);
        } else if (codecs && flag == "--set") {
            if (!add_setting(value, o.settings))
                return std::nullopt;
        } else if (collect && (flag == "--cap" || flag == "--grace")) {
            const auto n = parse_count(value);
            if (!n)
                return std::nullopt;
            if (flag == "--cap")
                o.collection.cap = *n;
            else
                o.collection.grace = std::chrono::seconds(*n);
        } else if (ids && flag == "--verify") {
            const auto n = parse_positive(value);
            if (!n && value != "all")
                return std::nullopt;
            o.verify = n ? *n : std::numeric_limits<std::uint64_t>::max();
        } else if (models && flag == "--dump") {
            o.model_dump = absolute_path(value);
        } else if (render && flag == "--oracle") {
            o.oracle = absolute_path(value);
        } else if ((render || html) && flag == "--out") {
            o.render_out = absolute_path(value);
        } else if (render && flag == "--format") {
            o.format_store = absolute_path(value);
        } else {
            return std::nullopt;
        }
    }
    if (includes && (o.compile_commands.empty() || o.source_root.empty()))
        return std::nullopt;
    if (typed && o.meta.empty())
        return std::nullopt;
    if (!o.output_root.empty() && !o.diff_root.empty())
        return std::nullopt;
    // Compile commands hold absolute paths, so every path that is compared
    // against them must be absolute too, or rebasing silently matches nothing.
    o.checkout = absolute_path(o.checkout);
    if (includes) {
        o.compile_commands = absolute_path(o.compile_commands);
        o.source_root = absolute_path(o.source_root);
        for (auto& dir : o.implicit_system)
            dir = absolute_path(dir).generic_string();
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

int run_types(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto stage = ops::run_types_stage(o.checkout, table, o.meta,
        o.threads, o.repetitions);
    io::write_types_report(std::cout, stage, setup, table);
    return io::failed(stage) ? 1 : 0;
}

std::filesystem::path objects_of(const options& o) {
    return o.store.empty()
        ? compass::store::io::packs_dir(
            compass::store::io::git_common_dir(o.checkout))
        : o.store;
}

int run_store(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto objects = objects_of(o);
    const auto stage = ops::run_store_stage(o.checkout, table, objects,
        o.threads, o.repetitions);
    io::write_store_report(std::cout, stage, setup, objects);
    return io::failed(stage) ? 1 : 0;
}

int run_warm(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto objects = objects_of(o);
    const auto stage = ops::run_warm_stage(o.checkout, table, objects,
        o.threads, o.repetitions);
    io::write_warm_report(std::cout, stage, setup, objects);
    return io::failed(stage) ? 1 : 0;
}

int run_codecs(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto objects = objects_of(o);
    const auto stage = ops::run_codecs_stage(o.checkout, table, o.meta,
        objects, o.settings, o.threads, o.repetitions, o.output_root);
    io::write_codecs_report(std::cout, stage, setup, objects);
    return io::failed(stage) ? 1 : 0;
}

/**
 * @brief Prints the diff of the outputs against the diff root on standard
 * output, so it can be piped, and the summary on standard error.
 */
int run_codecs_diff(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto stage = ops::diff_codecs_stage(o.checkout, table, o.meta,
        objects_of(o), o.settings, o.threads, o.repetitions, o.diff_root);
    std::cout << stage.diff;
    io::write_codecs_diff_summary(std::cerr, stage, setup);
    return io::diff_status(stage);
}

int run_gc(const options& o) {
    const auto objects = objects_of(o);
    const auto result =
        ops::run_gc_stage(o.checkout, o.collection);
    io::write_gc_report(std::cout, result, o.collection, objects);
    return 0;
}

int run_ids(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto stage = ops::run_ids_stage(o.checkout, table, o.threads,
        o.repetitions, o.verify);
    io::write_ids_report(std::cout, stage, setup);
    return io::failed(stage) ? 1 : 0;
}

int run_models(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto result = ops::run_models_stage(o.checkout, table,
        o.repetitions, o.model_dump);
    ops::write_models_report(std::cout, result);
    return 0;
}

/**
 * @brief The codecs of the compass executable with the settings the
 * configuration file of the checkout gives them.
 * @throws std::runtime_error when the file has a problem.
 */
compass::codec::types::registry configured_codecs(
    const std::filesystem::path& checkout) {
    namespace config = compass::cli::config;
    const auto file = config::read_repository_file(checkout);
    if (!file)
        throw std::runtime_error(file.error());
    const auto c = config::resolve(compass::cli::app::codecs(), checkout,
        checkout / config::repository_file_name, *file, {});
    if (!c.problems.empty())
        throw std::runtime_error(c.problems.front());
    return compass::codec::ops::make_registry(compass::cli::app::codecs(), {},
        config::settings_of(c));
}

int run_render(const options& o) {
    io::setup_timing setup;
    const auto table = list_checkout(o, setup);
    const auto codecs = configured_codecs(o.checkout);
    const auto codegen = std::ranges::find(codecs.codecs,
        compass::codegen::codecs::codegen.name,
        &compass::codec::types::descriptor::name);
    const compass::codec::types::codec_id id{static_cast<std::uint32_t>(
        codegen - codecs.codecs.begin())};
    const compass::codec::types::option_values options{codegen->options,
        compass::codec::ops::option_values(codecs, id)};
    const auto result = ops::run_render_stage(o.checkout, table, options,
        o.oracle, o.repetitions, o.render_out, o.format_store, o.threads);
    ops::write_render_report(std::cout, result);
    return result.messages.empty() ? 0 : 1;
}

int run_html(const options& o) {
    const auto result = ops::run_html_stage(o.checkout, o.render_out,
        o.repetitions);
    ops::write_html_report(std::cout, result);
    return 0;
}

int run_search(const options& o) {
    const auto results = ops::run_search_stage(o.conninfo, o.environment,
        ops::read_questions(o.questions), o.ranking, o.repetitions);
    ops::write_search_report(std::cout, results);
    return 0;
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
    case stage::types: return run_types(o);
    case stage::store: return run_store(o);
    case stage::warm: return run_warm(o);
    case stage::codecs:
        return o.diff_root.empty() ? run_codecs(o) : run_codecs_diff(o);
    case stage::gc: return run_gc(o);
    case stage::ids: return run_ids(o);
    case stage::models: return run_models(o);
    case stage::render: return run_render(o);
    case stage::html: return run_html(o);
    case stage::search: return run_search(o);
    case stage::org_dump: return run_org_dump(o);
    }
    return 2;
}

}

int main(int argc, char** argv) {
    compass::platform::process::use_binary_standard_streams();
    const auto o = parse_options(argc, argv);
    if (!o) {
        std::cerr << usage;
        return 2;
    }
    try {
        return run(*o);
    } catch (const std::exception& e) {
        std::cerr << "compass-bench: " << e.what() << "\n";
        // Diff mode keeps 1 for "would change", so trouble is 2 there.
        return o->diff_root.empty() ? 1 : 2;
    }
}
