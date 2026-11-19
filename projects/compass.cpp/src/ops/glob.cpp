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
#include "compass.cpp/ops/glob.hpp"

#include <algorithm>
#include <expected>
#include "compass.cpp/ops/paths.hpp"

namespace compass::cpp::ops {

std::vector<std::string_view> glob(
    const compass::codec::types::corpus_view& v, std::string_view pattern) {
    const auto wild = pattern.find_first_of("*?");
    const auto slash = pattern.rfind('/', wild);
    const auto prefix = slash == std::string_view::npos
        ? std::string_view{} : pattern.substr(0, slash + 1);
    const auto last = pattern.find_last_of("*?/");
    const auto suffix = last == std::string_view::npos
        ? pattern : pattern.substr(last + 1);
    auto out = v.list_files(prefix, suffix);
    std::erase_if(out, [&](auto p) { return !glob_matches(pattern, p); });
    std::ranges::sort(out, component_less);
    return out;
}

namespace {

/**
 * @brief @p glob with each {a,b} replaced by each alternative in turn, or
 * why it cannot be.
 */
std::expected<std::vector<std::string>, std::string> expand_braces(
    std::string_view glob) {
    const auto open = glob.find('{');
    if (open == std::string_view::npos) {
        if (glob.find('}') != std::string_view::npos)
            return std::unexpected("unopened } in " + std::string(glob));
        return std::vector<std::string>{std::string(glob)};
    }
    const auto close = glob.find('}', open);
    if (close == std::string_view::npos)
        return std::unexpected("unclosed { in " + std::string(glob));
    const auto inner = glob.substr(open + 1, close - open - 1);
    if (inner.find('{') != std::string_view::npos)
        return std::unexpected("nested { in " + std::string(glob));
    auto rest = expand_braces(glob.substr(close + 1));
    if (!rest)
        return rest;
    std::vector<std::string> out;
    std::size_t at = 0;
    while (true) {
        const auto comma = std::min(inner.find(',', at), inner.size());
        for (const auto& r : *rest)
            out.push_back(std::string(glob.substr(0, open)) +
                std::string(inner.substr(at, comma - at)) + r);
        if (comma == inner.size())
            break;
        at = comma + 1;
    }
    return out;
}

}

std::expected<void, std::string> add_globs(std::string_view value,
    std::vector<std::string>& out) {
    std::size_t at = 0;
    while (at < value.size()) {
        const auto start = value.find_first_not_of(' ', at);
        if (start == std::string_view::npos)
            break;
        const auto end = std::min(value.find(' ', start), value.size());
        auto expanded = expand_braces(value.substr(start, end - start));
        if (!expanded)
            return std::unexpected(expanded.error());
        for (auto& g : *expanded) {
            if (g == "**" || g.ends_with("/**"))
                g += "/*";
            out.push_back(std::move(g));
        }
        at = end;
    }
    return {};
}

std::vector<std::string_view> covered_files(
    const compass::codec::types::corpus_view& v,
    std::span<const std::string> files, std::span<const std::string> outside) {
    std::vector<std::string_view> out;
    for (const auto& g : files) {
        for (const auto p : glob(v, g)) {
            if (std::ranges::none_of(outside, [&](const std::string& o) {
                    return glob_matches(o, p); }))
                out.push_back(p);
        }
    }
    std::ranges::sort(out);
    const auto [first, last] = std::ranges::unique(out);
    out.erase(first, last);
    return out;
}

}
