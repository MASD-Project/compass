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
#ifndef COMPASS_CODEGEN_TESTS_TEST_CHECKOUT_HPP
#define COMPASS_CODEGEN_TESTS_TEST_CHECKOUT_HPP

#include <deque>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/types/corpus.hpp"
#include "compass.ingest/meta/type_registry_builder.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codegen::tests {

using files = std::map<std::string, std::string, std::less<>>;

/**
 * @brief A checkout of @p all: its .org files parsed into a corpus typed
 * by the model types, and every file listed.
 */
class checkout final {
public:
    explicit checkout(const files& all) : bytes_(all) {
        ingest::meta::type_registry_builder builder;
        for (const auto name : ingest::meta::model_types)
            builder.add_model(name);
        types_ = builder.build();
        for (const auto& [path, text] : bytes_) {
            files_.paths.push_back(path);
            if (!path.ends_with(".org"))
                continue;
            auto& t = tables_.emplace_back();
            ingest::org::parse(text, t);
            const auto view = org::types::view(t);
            corpus_.paths.push_back(path);
            corpus_.contents.push_back(store::ops::blob_id_of(text));
            corpus_.sources.push_back(text);
            corpus_.tables.push_back(view);
            corpus_.types.push_back(codec::ops::document_type(text, view,
                types_));
        }
        files_.content_of = [this](std::string_view p) {
            return std::optional(store::ops::blob_id_of(
                bytes_.find(p)->second));
        };
        files_.read = [this](std::string_view p) {
            return std::optional(bytes_.find(p)->second);
        };
        corpus_.files = &files_;
    }
    checkout(const checkout&) = delete;
    checkout& operator=(const checkout&) = delete;

    [[nodiscard]] const codec::types::corpus& corpus() const {
        return corpus_;
    }
    [[nodiscard]] const org::types::type_registry& types() const {
        return types_;
    }

private:
    files bytes_;
    org::types::type_registry types_;
    std::deque<org::types::tables> tables_;
    codec::types::checkout_files files_;
    codec::types::corpus corpus_;
};

}

#endif
