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
#include "compass.bench/ops/search_stage.hpp"

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <stdexcept>
#include "compass.db/io/connection.hpp"
#include "compass.db/query/search.hpp"

namespace compass::bench::ops {

namespace {

/**
 * @brief Enough hits that a page ranked tenth among documents is still
 * among them when its document has many matching sections.
 */
constexpr std::uint32_t hits_asked = 200;

std::vector<std::string> split(std::string_view s, char by) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at <= s.size()) {
        const auto end = std::min(s.find(by, at), s.size());
        if (end > at)
            out.emplace_back(s.substr(at, end - at));
        at = end + 1;
    }
    return out;
}

double percentile(std::vector<double> v, double p) {
    if (v.empty())
        return 0;
    std::ranges::sort(v);
    const auto i = static_cast<std::size_t>(
        p * static_cast<double>(v.size() - 1) + 0.5);
    return v[std::min(i, v.size() - 1)];
}

struct scores {
    std::size_t asked = 0;
    std::size_t at1 = 0;
    std::size_t at5 = 0;
    double reciprocal = 0;

    void add(std::size_t rank) {
        ++asked;
        if (rank == 1)
            ++at1;
        if (rank != 0 && rank <= 5)
            ++at5;
        if (rank != 0)
            reciprocal += 1.0 / static_cast<double>(rank);
    }
};

void write_scores(std::ostream& os, std::string_view label, const scores& s) {
    const auto n = static_cast<double>(std::max<std::size_t>(s.asked, 1));
    os << std::format("{:<10} questions {:>2}  hit@1 {:.2f}  hit@5 {:.2f}  "
        "MRR {:.3f}\n", label, s.asked, static_cast<double>(s.at1) / n,
        static_cast<double>(s.at5) / n, s.reciprocal / n);
}

}

std::vector<question> read_questions(const std::filesystem::path& file) {
    std::ifstream in(file);
    if (!in)
        throw std::runtime_error("cannot read " + file.string());
    std::vector<question> out;
    std::string line;
    while (std::getline(in, line)) {
        const auto fields = split(line, '\t');
        if (fields.size() != 3)
            continue;
        out.push_back({fields[0], fields[1], split(fields[2], ' ')});
    }
    return out;
}

std::vector<question_result> run_search_stage(const std::string& conninfo,
    const std::string& environment, const std::vector<question>& questions,
    const db::types::ranking& how, unsigned repetitions) {
    db::io::connection c(conninfo);
    const db::types::view_filter team{"true"};
    std::vector<question_result> out;
    for (const auto& q : questions) {
        db::types::search_request page;
        page.environment = environment;
        page.q = q.query;
        question_result result;
        result.asked = q;
        const auto timed = [&] {
            const auto start = std::chrono::steady_clock::now();
            static_cast<void>(db::query::search(c, team, page, how));
            return std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count();
        };
        result.first_ms = timed();
        for (unsigned i = 0; i < repetitions; ++i)
            result.warm_ms.push_back(timed());

        auto deep = page;
        deep.limit = hits_asked;
        const auto response = db::query::search(c, team, deep, how);
        result.total = response.total;
        std::vector<std::string> seen;
        for (const auto& h : response.hits) {
            if (std::ranges::contains(seen, h.org_id))
                continue;
            seen.push_back(h.org_id);
            if (result.top.size() < 3)
                result.top.push_back(h.title);
            if (result.rank == 0 && std::ranges::contains(q.answers, h.org_id))
                result.rank = seen.size();
        }
        out.push_back(std::move(result));
    }
    return out;
}

void write_search_report(std::ostream& os,
    const std::vector<question_result>& results) {
    scores all;
    scores developer;
    scores business;
    std::vector<double> first;
    std::vector<double> warm;
    for (const auto& r : results) {
        os << std::format("{:>3} {:<9} {:<38} hits {:>5} {:>6.1f} ms | ",
            r.rank == 0 ? std::string("-") : std::to_string(r.rank),
            r.asked.audience, r.asked.query, r.total,
            percentile(r.warm_ms, 0.5));
        for (std::size_t i = 0; i < r.top.size(); ++i)
            os << (i == 0 ? "" : " | ") << r.top[i];
        os << "\n";
        all.add(r.rank);
        (r.asked.audience == "developer" ? developer : business).add(r.rank);
        first.push_back(r.first_ms);
        warm.insert(warm.end(), r.warm_ms.begin(), r.warm_ms.end());
    }
    os << "\n";
    write_scores(os, "all", all);
    write_scores(os, "developer", developer);
    write_scores(os, "business", business);
    os << std::format("latency first run: median {:.1f} ms, p95 {:.1f} ms "
        "over {} queries\n", percentile(first, 0.5), percentile(first, 0.95),
        first.size());
    os << std::format("latency warm:      median {:.1f} ms, p95 {:.1f} ms "
        "over {} queries\n", percentile(warm, 0.5), percentile(warm, 0.95),
        warm.size());
}

}
