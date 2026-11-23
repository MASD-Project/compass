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
#include "compass.db/ops/copy_text.hpp"

#include <format>
#include "compass.platform/text/decoding.hpp"

namespace compass::db::ops {

namespace {

/**
 * @brief Appends @p value with COPY's escapes, dropping NUL bytes.
 */
void append_escaped(std::string& out, std::string_view value) {
    for (const char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\0': break;
        default: out += c;
        }
    }
}

}

void copy_writer::separate() {
    if (!first_)
        out_ += '\t';
    first_ = false;
}

void copy_writer::escaped(std::string_view value) {
    if (platform::text::is_utf8(value))
        append_escaped(out_, value);
    else
        append_escaped(out_, platform::text::decode_replacing(value));
}

void copy_writer::text(std::string_view value) {
    separate();
    escaped(value);
}

void copy_writer::text(const std::optional<std::string>& value) {
    if (value)
        text(*value);
    else
        null();
}

void copy_writer::number(std::uint64_t value) {
    separate();
    out_ += std::to_string(value);
}

void copy_writer::integer(std::int64_t value) {
    separate();
    out_ += std::to_string(value);
}

void copy_writer::null() {
    separate();
    out_ += "\\N";
}

void copy_writer::bytes(std::span<const std::uint8_t> value) {
    separate();
    out_ += "\\\\x";
    for (const auto b : value)
        out_ += std::format("{:02x}", b);
}

void copy_writer::array(std::span<const std::string> values) {
    std::string literal = "{";
    for (const auto& v : values) {
        if (literal.size() > 1)
            literal += ',';
        literal += '"';
        for (const char c : v) {
            if (c == '"' || c == '\\')
                literal += '\\';
            literal += c;
        }
        literal += '"';
    }
    literal += '}';
    separate();
    escaped(literal);
}

void copy_writer::end() {
    out_ += '\n';
    first_ = true;
}

}
