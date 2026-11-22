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
#ifndef COMPASS_BENCH_OPS_HTML_STAGE_HPP
#define COMPASS_BENCH_OPS_HTML_STAGE_HPP

#include <cstddef>
#include <filesystem>
#include <map>
#include <ostream>
#include <string>
#include <vector>

namespace compass::bench::ops {

/**
 * @brief What rendering the knowledge and manual pages of a tree did.
 *
 * scanned counts the .org files read and rendered the pages of type
 * knowledge or manual among them. parse_ms is the time to parse the
 * rendered pages once, render_ms one time per repetition of rendering them
 * all. bytes is the HTML one repetition wrote. links counts the id and
 * file links the renderer asked about; unknown counts those whose target is
 * not in the tree and hidden those whose target is outside the view, both
 * of which rendered as text. A page with no :ID: is not rendered. unsupported
 * counts each kind that fell to org-unsupported.
 */
struct html_result {
    std::size_t scanned = 0;
    std::size_t rendered = 0;
    std::size_t bytes = 0;
    std::size_t links = 0;
    std::size_t unknown = 0;
    std::size_t hidden = 0;
    double parse_ms = 0;
    std::vector<double> render_ms;
    std::map<std::string, std::size_t> unsupported;
};

/**
 * @brief Renders every page of type knowledge or manual under @p root,
 * @p repetitions times.
 *
 * Every .org file under @p root is read once to learn the IDs of documents
 * and headings, their paths and their types. A link resolves within the
 * tree, and only to a knowledge or manual page, as the knowledge view sees
 * it. With @p out set, each page is written there as an HTML file at its
 * path with .html appended.
 */
[[nodiscard]] html_result run_html_stage(const std::filesystem::path& root,
    const std::filesystem::path& out, unsigned repetitions);

void write_html_report(std::ostream& os, const html_result& r);

}

#endif
