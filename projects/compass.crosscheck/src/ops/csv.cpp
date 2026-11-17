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
#include "compass.crosscheck/ops/csv.hpp"

namespace compass::crosscheck::ops {

std::vector<std::vector<std::string>> csv_rows(std::string_view text,
    char delimiter) {
    enum class state { record, field, in_field, quoted, quote, eat_crnl };
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> fields;
    std::string field;
    auto s = state::record;
    const auto save = [&] {
        fields.push_back(std::move(field));
        field.clear();
    };
    constexpr int eol = -1;
    const auto process = [&](int c) {
        switch (s) {
        case state::record:
            if (c == eol)
                return;
            if (c == '\n' || c == '\r') {
                s = state::eat_crnl;
                return;
            }
            s = state::field;
            [[fallthrough]];
        case state::field:
            if (c == '\n' || c == '\r' || c == eol) {
                save();
                s = c == eol ? state::record : state::eat_crnl;
            } else if (c == '"') {
                s = state::quoted;
            } else if (c == delimiter) {
                save();
            } else {
                field.push_back(static_cast<char>(c));
                s = state::in_field;
            }
            return;
        case state::in_field:
            if (c == '\n' || c == '\r' || c == eol) {
                save();
                s = c == eol ? state::record : state::eat_crnl;
            } else if (c == delimiter) {
                save();
                s = state::field;
            } else {
                field.push_back(static_cast<char>(c));
            }
            return;
        case state::quoted:
            if (c == eol)
                return;
            if (c == '"')
                s = state::quote;
            else
                field.push_back(static_cast<char>(c));
            return;
        case state::quote:
            if (c == '"') {
                field.push_back('"');
                s = state::quoted;
            } else if (c == delimiter) {
                save();
                s = state::field;
            } else if (c == '\n' || c == '\r' || c == eol) {
                save();
                s = c == eol ? state::record : state::eat_crnl;
            } else {
                field.push_back(static_cast<char>(c));
                s = state::in_field;
            }
            return;
        case state::eat_crnl:
            if (c == '\n' || c == '\r')
                return;
            if (c == eol) {
                s = state::record;
                return;
            }
            throw csv_error("new-line character seen in unquoted field");
        }
    };
    std::size_t at = 0;
    while (at < text.size()) {
        auto end = text.find('\n', at);
        end = end == std::string_view::npos ? text.size() : end + 1;
        for (std::size_t i = at; i < end; ++i)
            process(static_cast<unsigned char>(text[i]));
        process(eol);
        at = end;
        if (s == state::record) {
            rows.push_back(std::move(fields));
            fields.clear();
        }
    }
    if (!field.empty() || s == state::quoted) {
        save();
        rows.push_back(std::move(fields));
    }
    return rows;
}

}
