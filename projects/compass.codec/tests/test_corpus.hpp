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
#ifndef COMPASS_CODEC_TESTS_TEST_CORPUS_HPP
#define COMPASS_CODEC_TESTS_TEST_CORPUS_HPP

#include <deque>
#include <initializer_list>
#include <string>
#include <string_view>
#include "compass.codec/ops/run.hpp"
#include "compass.codec/types/corpus.hpp"
#include "compass.ingest/meta/type_registry_builder.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.org/types/type_registry.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codec::tests {

inline constexpr std::string_view story_contract = R"(#+title: Document type: story

* Contract

- Folder :: =story.org=
- Level :: =s2=
- TODO vocabulary :: =BACKLOG STARTED | DONE=
- Required sections ::
  - =* Goal= — the outcome.
  - =* Status= — table.
  - =* Documents= — /Optional/; required once the story produces any.
)";

inline constexpr std::string_view task_contract = R"(#+title: Document type: task

* Contract

- Folder :: =task.org=
- Level :: =s1=
- TODO vocabulary :: =BACKLOG STARTED | DONE=
- Required sections ::
  - =* Goal= — one paragraph.
  - =* Result= — filled when the task closes.
)";

/**
 * @brief A corpus of parsed documents and the type registry of the given
 * contract pages, with everything the corpus points at kept alive.
 */
class test_corpus final {
public:
    test_corpus(std::initializer_list<std::string_view> contracts = {
        story_contract, task_contract}) {
        ingest::meta::type_registry_builder builder;
        org::types::tables page;
        for (const auto contract : contracts) {
            ingest::org::parse(contract, page);
            builder.add_contract(contract, org::types::view(page));
        }
        types_ = builder.build();
    }

    void add(std::string_view path, std::string_view source) {
        const auto& kept_path = paths_.emplace_back(path);
        const auto& kept_source = sources_.emplace_back(source);
        auto& tables = tables_.emplace_back();
        ingest::org::parse(kept_source, tables);
        const auto view = org::types::view(tables);
        corpus_.paths.emplace_back(kept_path);
        corpus_.contents.push_back(store::ops::blob_id_of(kept_source));
        corpus_.sources.emplace_back(kept_source);
        corpus_.tables.push_back(view);
        corpus_.types.push_back(ops::document_type(kept_source, view, types_));
    }

    [[nodiscard]] const types::corpus& corpus() const noexcept {
        return corpus_;
    }

    [[nodiscard]] const org::types::type_registry& types() const noexcept {
        return types_;
    }

private:
    org::types::type_registry types_;
    std::deque<std::string> paths_;
    std::deque<std::string> sources_;
    std::deque<org::types::tables> tables_;
    types::corpus corpus_;
};

}

#endif
