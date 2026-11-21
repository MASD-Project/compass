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
 * its source the finding is about. A finding about a file of the checkout
 * that is not a document has no row: document is none, file is the file's
 * path relative to the root of the checkout and range the bytes of that
 * file. message says what is wrong and fix what to change. The message
 * of a finding that is still there after an unrelated edit must not
 * change, so it holds no line number and no count: a run is compared with
 * another by path, rule and message.
 *
 * subject, when not empty, is the path of the file whose addition created
 * what the finding requires, such as the overview of a part that must be
 * registered, and subject_text the text the requirement is about, in the
 * subject; compass blame reads them, and nothing else does.
 */
struct diagnostic {
    compass::org::types::document_id document;
    compass::org::types::source_range range;
    rule_id rule;
    severity level = severity::error;
    std::string_view message;
    std::string_view fix;
    std::string_view file = {};
    std::string_view subject = {};
    std::string_view subject_text = {};
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
    text_column<diagnostic_id> files;
    text_column<diagnostic_id> subjects;
    text_column<diagnostic_id> subject_texts;

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
        files.add(d.file);
        subjects.add(d.subject);
        subject_texts.add(d.subject_text);
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
        files.append(other.files);
        subjects.append(other.subjects);
        subject_texts.append(other.subject_texts);
    }

    /**
     * @brief The finding of row @p d.
     */
    [[nodiscard]] diagnostic operator[](diagnostic_id d) const noexcept {
        return {documents[d], ranges[d], rules[d], levels[d], messages[d],
            fixes[d], files[d], subjects[d], subject_texts[d]};
    }
};

}

#endif
