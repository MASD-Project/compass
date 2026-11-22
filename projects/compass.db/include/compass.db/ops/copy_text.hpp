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
#ifndef COMPASS_DB_OPS_COPY_TEXT_HPP
#define COMPASS_DB_OPS_COPY_TEXT_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace compass::db::ops {

/**
 * @brief Writes rows in the text format of COPY ... FROM STDIN: fields
 * separated by tabs, rows ended by newlines, \N for NULL, and a backslash
 * escaping a backslash, a newline, a carriage return or a tab. Text that
 * is not UTF-8 has each bad byte replaced by U+FFFD, and NUL bytes, which
 * no PostgreSQL text holds, are dropped.
 */
class copy_writer final {
public:
    explicit copy_writer(std::string& out) noexcept : out_(out) {}

    void text(std::string_view value);
    void text(const std::string& value) { text(std::string_view(value)); }
    void text(const char* value) { text(std::string_view(value)); }
    void text(const std::optional<std::string>& value);
    void number(std::uint64_t value);
    void null();

    /**
     * @brief A bytea value, written in hex.
     */
    void bytes(std::span<const std::uint8_t> value);

    /**
     * @brief A text[] value.
     */
    void array(std::span<const std::string> values);

    /**
     * @brief Ends the row.
     */
    void end();

private:
    void separate();
    void escaped(std::string_view value);

    std::string& out_;
    bool first_ = true;
};

}

#endif
