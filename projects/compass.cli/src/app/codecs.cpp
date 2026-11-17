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
#include "compass.cli/app/codecs.hpp"

#include <vector>
#include "compass.codec/codecs/catalogue.hpp"
#include "compass.codec/ops/rule_table.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codegen/codecs/codegen.hpp"
#include "compass.codegen/codecs/component_files.hpp"
#include "compass.codegen/codecs/pending_models.hpp"
#include "compass.cpp/codecs/catalogue.hpp"
#include "compass.crosscheck/codecs/catalogue.hpp"
#include "compass.docs/codecs/catalogue.hpp"
#include "compass.sql/codecs/catalogue.hpp"

namespace compass::cli::app {

namespace {

void rule_table(const codec::types::corpus_view& v,
    codec::types::file_table& out) {
    out.add(v.option("path"), codec::ops::rule_table(codecs(), v));
}

}

std::span<const codec::types::descriptor> codecs() noexcept {
    static const auto all = [] {
        const auto shipped = codec::codecs::catalogue();
        std::vector<codec::types::descriptor> out(shipped.begin(),
            shipped.end());
        out.push_back(codegen::codecs::codegen);
        out.push_back(codegen::codecs::component_files);
        out.push_back(codegen::codecs::pending_models);
        for (const auto& d : cpp::codecs::catalogue())
            out.push_back(d);
        for (const auto& d : sql::codecs::catalogue())
            out.push_back(d);
        for (const auto& d : crosscheck::codecs::catalogue())
            out.push_back(d);
        for (const auto& d : docs::codecs::catalogue())
            out.push_back(d);
        out.push_back({
            .name = "rule_table",
            .version = codec::ops::rule_table_version(out),
            .summary = "Writes the page of every rule, the memories and "
                       "documents it cites, and the memories no rule cites.",
            .reads = codec::types::inputs::every_typed_document(),
            .options = codec::ops::rule_table_options,
            .rules = {},
            .enabled_by_default = false,
            .run = codec::types::corpus_function<codec::types::file_table>{
                rule_table}});
        return out;
    }();
    return all;
}

}
