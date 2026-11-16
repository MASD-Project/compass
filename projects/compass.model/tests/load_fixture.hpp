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
#ifndef COMPASS_MODEL_TESTS_LOAD_FIXTURE_HPP
#define COMPASS_MODEL_TESTS_LOAD_FIXTURE_HPP

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include "compass.ingest/org/parser.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/load.hpp"

namespace compass::model::tests {

/**
 * @brief A checkout held in memory: documents by path, parsed when added.
 */
class checkout final {
public:
    void add(std::string path, std::string source) {
        auto& d = documents_.emplace_back();
        d.path = std::move(path);
        d.source = std::move(source);
        compass::ingest::org::parse(d.source, d.tables);
        d.view = compass::org::types::view(d.tables);
        index_[d.path] = &d;
    }

    /**
     * @brief The canonical JSON of the model at @p path, loaded with every
     * other document of the checkout visible to it.
     */
    [[nodiscard]] std::string load_json(std::string_view path) const {
        const auto& d = *index_.at(std::string(path));
        const ops::document_finder find = [this](std::string_view p)
            -> std::optional<ops::document> {
            const auto it = index_.find(std::string(p));
            if (it == index_.end())
                return std::nullopt;
            return document_of(*it->second);
        };
        const auto doc = document_of(d);
        const auto type = ops::model_type_of(ops::declared_type(doc));
        types::model_tables tables;
        ops::builder b(tables);
        ops::load_cache cache;
        const auto id = ops::load_model(b, doc, *type, find, cache);
        std::string out;
        io::write_json(out, tables, tables.models.roots[id]);
        return out;
    }

private:
    struct parsed {
        std::string path;
        std::string source;
        compass::org::types::tables tables;
        compass::org::types::tables_view view;
    };

    static ops::document document_of(const parsed& p) {
        return {p.path, p.source, &p.view};
    }

    std::deque<parsed> documents_;
    std::map<std::string, const parsed*> index_;
};

}

#endif
