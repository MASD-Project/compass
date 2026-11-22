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
#include "compass.db/io/connection.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <libpq-fe.h>

namespace compass::db::io {

namespace {

/**
 * @brief The message of @p c's last failure, without libpq's final newline.
 */
std::string message_of(const PGconn* c) {
    std::string m = c == nullptr ? "out of memory" : PQerrorMessage(c);
    while (!m.empty() && (m.back() == '\n' || m.back() == ' '))
        m.pop_back();
    return m;
}

[[noreturn]] void fail(const PGconn* c, const PGresult* r) {
    const char* state = r == nullptr ? nullptr
                                     : PQresultErrorField(r, PG_DIAG_SQLSTATE);
    std::string m = r == nullptr ? message_of(c) : PQresultErrorMessage(r);
    while (!m.empty() && (m.back() == '\n' || m.back() == ' '))
        m.pop_back();
    if (m.empty())
        m = message_of(c);
    throw error(m, state == nullptr ? std::string() : std::string(state));
}

/**
 * @brief @p r, checked: a result that is not a command's or a query's
 * success is cleared and thrown.
 */
result checked(PGconn* c, PGresult* r) {
    const auto status = r == nullptr ? PGRES_FATAL_ERROR : PQresultStatus(r);
    if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK)
        return result(r);
    const result owned(r);
    fail(c, r);
}

/**
 * @brief Waits for every result of the statement sent last, so the
 * connection is ready for the next, and throws the first failure among
 * them. Returns the rows the last successful one touched.
 */
std::size_t drain(PGconn* c) {
    PGresult* failed = nullptr;
    std::size_t affected = 0;
    while (PGresult* r = PQgetResult(c)) {
        const auto status = PQresultStatus(r);
        if (status == PGRES_COMMAND_OK || status == PGRES_TUPLES_OK)
            affected = result(r).affected();
        else if (failed == nullptr)
            failed = r;
        else
            PQclear(r);
    }
    if (failed != nullptr) {
        const result owned(failed);
        fail(c, failed);
    }
    return affected;
}

void ignore_notice(void*, const char*) {}

}

result::~result() {
    if (r_ != nullptr)
        PQclear(r_);
}

result::result(result&& other) noexcept : r_(other.r_) {
    other.r_ = nullptr;
}

result& result::operator=(result&& other) noexcept {
    if (this != &other) {
        if (r_ != nullptr)
            PQclear(r_);
        r_ = other.r_;
        other.r_ = nullptr;
    }
    return *this;
}

std::size_t result::rows() const noexcept {
    return static_cast<std::size_t>(PQntuples(r_));
}

std::size_t result::columns() const noexcept {
    return static_cast<std::size_t>(PQnfields(r_));
}

bool result::is_null(std::size_t row, std::size_t column) const {
    return PQgetisnull(r_, static_cast<int>(row), static_cast<int>(column)) !=
        0;
}

std::string_view result::value(std::size_t row, std::size_t column) const {
    const auto r = static_cast<int>(row);
    const auto c = static_cast<int>(column);
    return {PQgetvalue(r_, r, c),
        static_cast<std::size_t>(PQgetlength(r_, r, c))};
}

std::size_t result::affected() const {
    const char* n = PQcmdTuples(r_);
    return n == nullptr || *n == '\0'
        ? 0
        : static_cast<std::size_t>(std::strtoull(n, nullptr, 10));
}

connection::connection(const std::string& conninfo) {
    const std::array<const char*, 4> keys{
        "connect_timeout", "application_name", "dbname", nullptr};
    const std::array<const char*, 4> values{
        "10", "compass", conninfo.c_str(), nullptr};
    c_ = PQconnectdbParams(keys.data(), values.data(), 1);
    if (c_ == nullptr || PQstatus(c_) != CONNECTION_OK) {
        const auto m = message_of(c_);
        if (c_ != nullptr)
            PQfinish(c_);
        c_ = nullptr;
        throw error("cannot connect to the database: " + m, "");
    }
    PQsetNoticeProcessor(c_, ignore_notice, nullptr);
}

connection::~connection() {
    if (c_ != nullptr)
        PQfinish(c_);
}

connection::connection(connection&& other) noexcept : c_(other.c_) {
    other.c_ = nullptr;
}

connection& connection::operator=(connection&& other) noexcept {
    if (this != &other) {
        if (c_ != nullptr)
            PQfinish(c_);
        c_ = other.c_;
        other.c_ = nullptr;
    }
    return *this;
}

result connection::exec(std::string_view sql,
    const std::vector<parameter>& parameters) {
    const std::string statement(sql);
    std::vector<const char*> values;
    values.reserve(parameters.size());
    for (const auto& p : parameters)
        values.push_back(p ? p->c_str() : nullptr);
    return checked(c_, PQexecParams(c_, statement.c_str(),
        static_cast<int>(values.size()), nullptr, values.data(), nullptr,
        nullptr, 0));
}

void connection::run_script(std::string_view sql) {
    const std::string script(sql);
    if (PQsendQuery(c_, script.c_str()) == 0)
        fail(c_, nullptr);
    static_cast<void>(drain(c_));
}

std::size_t connection::copy_in(std::string_view copy, std::string_view data) {
    const std::string statement(copy);
    {
        PGresult* started = PQexec(c_, statement.c_str());
        const result owned(started);
        if (started == nullptr || PQresultStatus(started) != PGRES_COPY_IN)
            fail(c_, started);
    }
    constexpr std::size_t chunk = 1 << 20;
    bool sent = true;
    for (std::size_t at = 0; sent && at < data.size(); at += chunk) {
        const auto n = std::min(chunk, data.size() - at);
        sent = PQputCopyData(c_, data.data() + at, static_cast<int>(n)) == 1;
    }
    if (PQputCopyEnd(c_, sent ? nullptr : "the client could not send") != 1)
        fail(c_, nullptr);
    return drain(c_);
}

transaction::transaction(connection& c) : c_(c) { c_.exec("BEGIN"); }

transaction::~transaction() {
    if (!open_)
        return;
    try {
        c_.exec("ROLLBACK");
    } catch (const error&) {
        // A connection that failed cannot roll back; closing it does.
    }
}

void transaction::commit() {
    open_ = false;
    c_.exec("COMMIT");
}

}
