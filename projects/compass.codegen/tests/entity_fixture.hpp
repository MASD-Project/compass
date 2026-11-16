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
#ifndef COMPASS_CODEGEN_TESTS_ENTITY_FIXTURE_HPP
#define COMPASS_CODEGEN_TESTS_ENTITY_FIXTURE_HPP

#include <string>
#include <string_view>
#include "compass.codegen/ops/values.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"

namespace compass::codegen::tests {

/**
 * @brief A domain entity read from JSON, as the model holds it (loaded)
 * and as the enrichment steps change it (entity, a copy).
 */
struct entity_fixture {
    model::types::model_tables tables;
    model::ops::builder b{tables};
    model::types::value_id loaded;
    model::types::value_id entity;

    explicit entity_fixture(std::string_view json) {
        loaded = model::io::read_json(b, json);
        const auto holder = b.detached_object();
        b.set_copy(holder, "domain_entity", loaded);
        entity = b.find(holder, "domain_entity");
    }

    /**
     * @brief The canonical JSON of the member @p key of @p object, without
     * its line end; <absent> when there is no such member.
     */
    std::string json(model::types::value_id object, std::string_view key) {
        const auto v = codegen::ops::values::member(tables, object, key);
        if (v.is_none())
            return "<absent>";
        std::string out;
        model::io::write_json(out, tables, v);
        out.pop_back();
        return out;
    }

    std::string json(std::string_view key) { return json(entity, key); }
};

}

#endif
