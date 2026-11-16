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
#include "compass.bench/ops/model_corpus.hpp"

#include <optional>
#include "compass.ingest/org/parser.hpp"

namespace compass::bench::ops {

bool is_model_candidate(std::string_view path) {
    if (!path.starts_with("projects/") || !path.ends_with(".org"))
        return false;
    const auto slash = path.rfind('/');
    const auto dir = path.substr(0, slash);
    return dir.ends_with("/modeling");
}

parsed_document* model_corpus::get(std::string_view path) {
    const auto it = index_.find(path);
    if (it != index_.end())
        return it->second;
    const auto full = checkout_ / std::filesystem::path(path);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(full, ec)) {
        index_.emplace(std::string(path), nullptr);
        return nullptr;
    }
    auto& p = documents_.emplace_back(parsed_document{std::string(path),
        compass::platform::filesystem::mapped_file(full), {}, {}});
    compass::ingest::org::parse(p.file.text(), p.tables);
    p.view = compass::org::types::view(p.tables);
    index_.emplace(std::string(path), &p);
    return &p;
}

compass::model::ops::document_finder model_corpus::finder() {
    return [this](std::string_view path)
        -> std::optional<compass::model::ops::document> {
        auto* p = get(path);
        if (p == nullptr)
            return std::nullopt;
        return document_of(*p);
    };
}

compass::model::ops::document document_of(const parsed_document& p) {
    return {p.path, p.file.text(), &p.view};
}

}
