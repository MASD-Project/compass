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
     * @brief Adds the documents of projects/modeling a model reads: the
     * simple-lookup profile and a custom type registry holding
     * cron_expression, each with the rows of ORE Studio's own that the
     * tests touch, so a model loads as it does there.
     */
    void add_modeling_documents() {
        add("projects/modeling/variability_simple_lookup.org",
            "#+title: variability_simple_lookup\n\n* Assignments\n\n"
            "| Feature | Value |\n|------+-------|\n"
            "| [[id:A7223BE6-4D0F-47CE-8E31-9D896D970CB3][has_tenant_id]] "
            "| true |\n"
            "| [[id:4F426ED0-E75D-47CC-B8E8-1775185D08D0][system_scope]] "
            "| false |\n"
            "| [[id:743EBB4F-46EE-48C2-AF8C-205FF7958AC2]"
            "[nullable_tenant_id]] | false |\n"
            "| [[id:9307F2AC-EFBE-4D4A-873A-C609CD94C363]"
            "[has_uuid_primary_key]] | false |\n"
            "| [[id:6EDA740C-D053-4E1B-98EB-E1AD87B6A639]"
            "[has_change_reason_cache]] | true |\n"
            "| [[id:26409919-470E-4820-B8C1-AC816181F894][has_pagination]] "
            "| true |\n");
        add("projects/modeling/cpp_custom_types.org",
            "#+title: cpp_custom_types\n\n* The registry\n\n"
            "| Type | Header |\n|------+--------|\n"
            "| =cron_expression= "
            "| =\"ores.scheduler.api/domain/cron_expression.hpp\"= |\n");
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
