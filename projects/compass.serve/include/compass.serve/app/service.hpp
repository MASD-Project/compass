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
#ifndef COMPASS_SERVE_APP_SERVICE_HPP
#define COMPASS_SERVE_APP_SERVICE_HPP

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include "compass.db/io/connection.hpp"
#include "compass.db/types/search.hpp"
#include "compass.serve/domain/caller.hpp"
#include "compass.serve/domain/http_request.hpp"
#include "compass.serve/domain/http_response.hpp"
#include "compass.serve/net/router.hpp"
#include "compass.serve/types/settings.hpp"

namespace compass::serve::app {

/**
 * @brief Connections to one database, each lent to one request at a time
 * and kept for the next.
 */
class connection_pool final {
public:
    explicit connection_pool(std::string conninfo);

    /// A connection lent until the lease ends.
    class lease final {
    public:
        lease(connection_pool& pool, std::unique_ptr<db::io::connection> c);
        ~lease();
        lease(const lease&) = delete;
        lease& operator=(const lease&) = delete;
        db::io::connection& operator*() const noexcept {
            return *c_;
        }

    private:
        connection_pool& pool_;
        std::unique_ptr<db::io::connection> c_;
    };

    /**
     * @brief A connection: an idle one, else a new one.
     * @throws db::io::error when a new one cannot be made.
     */
    [[nodiscard]] lease acquire();

private:
    std::string conninfo_;
    std::mutex mutex_;
    std::vector<std::unique_ptr<db::io::connection>> idle_;
};

/**
 * @brief The document service: the views it serves, compiled, and the
 * database it reads.
 */
class service final {
public:
    /**
     * @throws std::runtime_error when a view of @p s does not compile.
     */
    service(types::settings s, connection_pool& pool);

    /**
     * @brief Adds every read route of the operation models, and
     * GET /v1/catalogue, to @p r.
     */
    void add_read_routes(net::router& r);

    /**
     * @brief What a request must pass before any route sees it, as the
     * gateway position needs: its secret when one is set, and a view this
     * service serves. A request without the gateway's headers takes the local
     * caller when one is set.
     * @returns the refusal to answer, or nothing when the request may go on.
     */
    [[nodiscard]] std::optional<domain::http_response>
    admit(domain::http_request& request) const;

    /// The view @p c reads through. @throws domain::refusal no_view.
    [[nodiscard]] const db::types::view_filter& view_of(const domain::caller& c) const;

private:
    types::settings settings_;
    std::map<std::string, db::types::view_filter, std::less<>> views_;
    connection_pool& pool_;
};

/**
 * @brief The catalogue of @p r's routes, in the shape of ORE Studio's
 * route_catalogue, with each route's method, path, query parameters, body
 * schema and input schema, as GET /v1/catalogue answers it.
 */
[[nodiscard]] std::string catalogue_json(const net::router& r);

}

#endif
