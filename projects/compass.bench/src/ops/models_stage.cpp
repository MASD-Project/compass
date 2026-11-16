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
#include "compass.bench/ops/models_stage.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include "compass.bench/ops/model_corpus.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/load.hpp"

namespace compass::bench::ops {

namespace {

using compass::model::types::model_type;

double elapsed_ms(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

void dump(const std::filesystem::path& root,
    const compass::model::types::model_tables& t) {
    std::string text;
    for (std::uint32_t m = 0; m < t.models.size(); ++m) {
        const compass::model::types::model_id id{m};
        const auto path = root / (std::string(compass::model::types::text_of(
            t.texts, t.models.paths[id])) + ".json");
        std::filesystem::create_directories(path.parent_path());
        text.clear();
        compass::model::io::write_json(text, t, t.models.roots[id]);
        std::ofstream os(path, std::ios::binary);
        os << text;
        if (!os)
            throw std::runtime_error("cannot write " + path.string());
    }
}

}

models_result run_models_stage(const std::filesystem::path& checkout,
    const types::file_table& table, unsigned repetitions,
    const std::filesystem::path& dump_root) {
    models_result r;
    model_corpus documents(checkout, table.paths);
    std::vector<std::pair<parsed_document*, model_type>> models;

    const auto start = std::chrono::steady_clock::now();
    for (const auto& path : table.paths) {
        if (!is_model_candidate(path))
            continue;
        auto* p = documents.get(path);
        if (p == nullptr)
            continue;
        ++r.documents;
        const auto type = compass::model::ops::model_type_of(
            compass::model::ops::declared_type(document_of(*p)));
        if (type)
            models.emplace_back(p, *type);
    }
    r.parse_ms = elapsed_ms(start);
    r.models = models.size();

    const auto find = documents.finder();
    const auto list = documents.lister();

    compass::model::types::model_tables tables;
    compass::model::ops::load_cache cache;
    for (unsigned rep = 0; rep < repetitions; ++rep) {
        tables = {};
        compass::model::ops::builder b(tables);
        r.by_type = {};
        r.errors = {};
        r.messages.clear();
        const auto load_start = std::chrono::steady_clock::now();
        for (const auto& [p, type] : models) {
            std::string error;
            compass::model::ops::load_model(b, document_of(*p), type, find,
                list, cache, &error);
            const auto t = static_cast<std::size_t>(type);
            ++r.by_type[t];
            if (!error.empty()) {
                ++r.errors[t];
                r.messages.push_back(p->path + ": " + error);
            }
        }
        r.load_ms.push_back(elapsed_ms(load_start));
    }
    r.values = tables.values.size();
    if (!dump_root.empty())
        dump(dump_root, tables);
    return r;
}

void write_models_report(std::ostream& os, const models_result& r) {
    os << "documents parsed: " << r.documents << " in " << r.parse_ms
       << " ms\n";
    os << "models: " << r.models << ", values: " << r.values << "\n";
    for (std::size_t i = 0; i < model_type_count; ++i) {
        if (r.by_type[i] == 0)
            continue;
        os << "  " << r.by_type[i] << " "
           << compass::model::types::name_of(
               compass::model::types::model_types[i]);
        if (r.errors[i] != 0)
            os << " (" << r.errors[i] << " refused)";
        os << "\n";
    }
    auto sorted = r.load_ms;
    std::ranges::sort(sorted);
    if (!sorted.empty()) {
        os << "load ms: min " << sorted.front() << ", median "
           << sorted[sorted.size() / 2] << ", max " << sorted.back()
           << " over " << sorted.size() << " runs\n";
    }
}

}
