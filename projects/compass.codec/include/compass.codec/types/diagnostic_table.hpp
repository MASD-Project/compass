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
#ifndef COMPASS_CODEC_TYPES_DIAGNOSTIC_TABLE_HPP
#define COMPASS_CODEC_TYPES_DIAGNOSTIC_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <string_view>
#include "compass.codec/types/text_column.hpp"
#include "compass.org/types/column.hpp"
#include "compass.org/types/handle.hpp"
#include "compass.org/types/source_range.hpp"

namespace compass::codec::types {

using diagnostic_id = compass::org::types::handle<struct diagnostic_tag>;

/**
 * @brief One of the rules a codec declares, by its position in the
 * descriptor's rules.
 */
using rule_id = compass::org::types::handle<struct rule_tag>;

enum class severity : std::uint8_t { warning, error };

/**
 * @brief One finding, as a codec states it.
 *
 * document is the row of the document in the corpus and range the bytes of
 * its source the finding is about. message says what is wrong and fix what
 * to change. The message of a finding that is still there after an
 * unrelated edit must not change, so it holds no line number and no count:
 * a run is compared with another by document path, rule and message.
 */
struct diagnostic {
    compass::org::types::document_id document;
    compass::org::types::source_range range;
    rule_id rule;
    severity level = severity::error;
    std::string_view message;
    std::string_view fix;
};

/**
 * @brief The findings a codec produced, one row per finding.
 */
struct diagnostic_table {
    compass::org::types::column<diagnostic_id,
        compass::org::types::document_id> documents;
    compass::org::types::column<diagnostic_id,
        compass::org::types::source_range> ranges;
    compass::org::types::column<diagnostic_id, rule_id> rules;
    compass::org::types::column<diagnostic_id, severity> levels;
    text_column<diagnostic_id> messages;
    text_column<diagnostic_id> fixes;

    [[nodiscard]] std::size_t size() const noexcept {
        return documents.size();
    }

    void add(const diagnostic& d) {
        documents.values.push_back(d.document);
        ranges.values.push_back(d.range);
        rules.values.push_back(d.rule);
        levels.values.push_back(d.level);
        messages.add(d.message);
        fixes.add(d.fix);
    }

    void append(const diagnostic_table& other) {
        const auto extend = [](auto& to, const auto& from) {
            to.values.insert(to.values.end(), from.values.begin(),
                from.values.end());
        };
        extend(documents, other.documents);
        extend(ranges, other.ranges);
        extend(rules, other.rules);
        extend(levels, other.levels);
        messages.append(other.messages);
        fixes.append(other.fixes);
    }
};

}

#endif
