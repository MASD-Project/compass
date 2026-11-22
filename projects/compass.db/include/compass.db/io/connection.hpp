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
#ifndef COMPASS_DB_IO_CONNECTION_HPP
#define COMPASS_DB_IO_CONNECTION_HPP

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct pg_conn;
struct pg_result;

namespace compass::db::io {

/**
 * @brief A failure the server or libpq reported: the message, and the
 * SQLSTATE code when the server gave one.
 */
class error : public std::runtime_error {
public:
    error(const std::string& message, std::string sqlstate)
        : std::runtime_error(message), sqlstate_(std::move(sqlstate)) {}

    [[nodiscard]] const std::string& sqlstate() const noexcept {
        return sqlstate_;
    }

private:
    std::string sqlstate_;
};

/**
 * @brief The rows a statement returned, in text form. Owns the result and
 * clears it on destruction. Move-only.
 */
class result final {
public:
    explicit result(pg_result* r) noexcept : r_(r) {}
    ~result();
    result(const result&) = delete;
    result& operator=(const result&) = delete;
    result(result&& other) noexcept;
    result& operator=(result&& other) noexcept;

    [[nodiscard]] std::size_t rows() const noexcept;
    [[nodiscard]] std::size_t columns() const noexcept;
    [[nodiscard]] bool is_null(std::size_t row, std::size_t column) const;

    /**
     * @brief The text of a value, empty for NULL; valid while the result
     * lives.
     */
    [[nodiscard]] std::string_view value(std::size_t row,
        std::size_t column) const;

    /**
     * @brief The number of rows an INSERT, UPDATE or DELETE touched.
     */
    [[nodiscard]] std::size_t affected() const;

private:
    pg_result* r_ = nullptr;
};

/**
 * @brief A statement parameter in text form; nothing is SQL NULL.
 */
using parameter = std::optional<std::string>;

/**
 * @brief One connection to a PostgreSQL server, opened from a libpq
 * connection string and closed on destruction. Move-only. Notices the
 * server sends, such as those of CREATE ... IF NOT EXISTS, are dropped.
 */
class connection final {
public:
    /**
     * @brief Connects with @p conninfo, a libpq connection string or URI.
     * A connection that cannot be made in ten seconds fails, unless
     * @p conninfo sets connect_timeout.
     * @throws error when the connection fails.
     */
    explicit connection(const std::string& conninfo);
    ~connection();
    connection(const connection&) = delete;
    connection& operator=(const connection&) = delete;
    connection(connection&& other) noexcept;
    connection& operator=(connection&& other) noexcept;

    /**
     * @brief Runs one statement with @p parameters bound to $1, $2, ...
     * @throws error when the statement fails.
     */
    result exec(std::string_view sql,
        const std::vector<parameter>& parameters = {});

    /**
     * @brief Runs a script of any number of statements, with no
     * parameters, as one round trip.
     * @throws error when a statement fails.
     */
    void run_script(std::string_view sql);

    /**
     * @brief Runs @p copy, a COPY ... FROM STDIN statement, sending
     * @p data, rows in COPY's text format.
     * @returns the number of rows copied.
     * @throws error when the copy fails.
     */
    std::size_t copy_in(std::string_view copy, std::string_view data);

private:
    pg_conn* c_ = nullptr;
};

/**
 * @brief A transaction on a connection: BEGIN on construction, ROLLBACK on
 * destruction unless committed.
 */
class transaction final {
public:
    explicit transaction(connection& c);
    ~transaction();
    transaction(const transaction&) = delete;
    transaction& operator=(const transaction&) = delete;

    /**
     * @throws error when the commit fails; the transaction is then over.
     */
    void commit();

private:
    connection& c_;
    bool open_ = true;
};

}

#endif
