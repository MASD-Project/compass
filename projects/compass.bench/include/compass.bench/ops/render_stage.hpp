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
#ifndef COMPASS_BENCH_OPS_RENDER_STAGE_HPP
#define COMPASS_BENCH_OPS_RENDER_STAGE_HPP

#include <cstddef>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>
#include "compass.bench/types/file_table.hpp"
#include "compass.codec/io/format_cache.hpp"
#include "compass.codec/types/formatter.hpp"

namespace compass::bench::ops {

/**
 * @brief What rendering the codegen outputs of a checkout did.
 *
 * templates counts the .mustache files compiled once, in compile_ms.
 * outputs and bytes count what one repetition rendered. setup_ms is the
 * time to build or read the contexts, render_ms one time per repetition of
 * rendering every output. messages names each output that failed. format
 * says what formatting the outputs did, when they were formatted.
 */
struct render_result {
    std::size_t templates = 0;
    std::size_t outputs = 0;
    std::size_t bytes = 0;
    std::size_t contexts = 0;
    double compile_ms = 0;
    double setup_ms = 0;
    std::vector<double> render_ms;
    std::vector<std::string> messages;
    std::optional<compass::codec::io::format_stats> format;
};

/**
 * @brief Renders every output of the checkout @p repetitions times.
 *
 * With @p oracle empty, the outputs and their contexts are the ones
 * compass.codegen builds from the models compass.model loads. With
 * @p oracle naming a dump of build/scripts/codegen_render_dump.py, they are
 * the dump's outputs rendered from the dump's contexts, which checks the
 * renderer apart from the contexts. When @p out is not empty, each output
 * is written to <out>/renders/<path>, the dump's layout.
 *
 * When @p format_store is not empty, the outputs are then formatted as
 * ores.codegen formats them, through the result cache of the git common
 * directory @p format_store, @p threads formatter processes at a time, and
 * each formatted output is written to <out>/formatted/<path>.
 */
[[nodiscard]] render_result run_render_stage(
    const std::filesystem::path& checkout, const types::file_table& table,
    const std::filesystem::path& oracle, unsigned repetitions,
    const std::filesystem::path& out,
    const std::filesystem::path& format_store, unsigned threads);

/**
 * @brief The formatters ores.codegen runs over what it writes, as
 * generate.py's clang_format_files and prettier_format_files run them:
 * clang-format twice over C++ with the checkout's .clang-format, and
 * prettier once over TypeScript with its .prettierrc.yaml, the workspace's
 * own prettier first. A program that cannot be found is left out.
 */
[[nodiscard]] std::vector<compass::codec::types::formatter> ores_formatters(
    const std::filesystem::path& checkout);

void write_render_report(std::ostream& os, const render_result& r);

}

#endif
