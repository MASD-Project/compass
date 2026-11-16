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
#include <string_view>
#include <thread>
#include "compass.bench/io/report.hpp"
#include "compass.bench/ops/listing.hpp"
#include "compass.bench/ops/partition.hpp"
#include "compass.bench/ops/read_stage.hpp"

namespace {

using namespace compass::bench;

constexpr std::string_view usage =
    "usage: compass-bench read <checkout> [-j N] [--repeat R]\n";

struct options {
    std::filesystem::path checkout;
    unsigned threads = 0;
    unsigned repetitions = 5;
};

std::optional<unsigned> parse_positive(std::string_view s) {
    unsigned v = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc{} || end != s.data() + s.size() || v == 0)
        return std::nullopt;
    return v;
}

std::optional<options> parse_options(int argc, char** argv) {
    if (argc < 3 || std::string_view(argv[1]) != "read")
        return std::nullopt;
    options o;
    o.checkout = argv[2];
    o.threads = std::max(1u, std::thread::hardware_concurrency());
    for (int i = 3; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (i + 1 >= argc)
            return std::nullopt;
        const auto value = parse_positive(argv[++i]);
        if (!value)
            return std::nullopt;
        if (flag == "-j")
            o.threads = *value;
        else if (flag == "--repeat")
            o.repetitions = *value;
        else
            return std::nullopt;
    }
    return o;
}

double elapsed_ms(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

int run(const options& o) {
    io::setup_timing setup;
    setup.threads = o.threads;

    auto start = std::chrono::steady_clock::now();
    const auto raw = ops::git_ls_files(o.checkout);
    const auto entries = ops::parse_listing(raw);
    setup.listing_ms = elapsed_ms(start);

    start = std::chrono::steady_clock::now();
    const auto table = ops::build_file_table(o.checkout, entries);
    setup.table_ms = elapsed_ms(start);

    const auto ranges = ops::balance_ranges(table.sizes, o.threads);
    const auto stage =
        ops::run_read_stage(o.checkout, table, ranges, o.repetitions);

    io::write_report(std::cout, stage, setup, table);
    return io::failed(stage) ? 1 : 0;
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
