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
#ifndef COMPASS_SERVE_APP_HTTP_SERVER_HPP
#define COMPASS_SERVE_APP_HTTP_SERVER_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include "compass.serve/domain/http_request.hpp"
#include "compass.serve/domain/http_response.hpp"
#include "compass.serve/net/router.hpp"

namespace compass::serve::app {

/**
 * @brief Where and how the server listens. port 0 takes a free port.
 * max_body is the largest body a route that states none reads.
 */
struct server_options {
    std::string address = "127.0.0.1";
    std::uint16_t port = 8080;
    unsigned threads = 4;
    std::uint64_t max_body = 1024 * 1024;
};

/**
 * @brief What a request must pass before any route sees it; the refusal to
 * answer, or nothing.
 */
using admission = std::function<std::optional<domain::http_response>(domain::http_request&)>;

/**
 * @brief A small HTTP/1.1 server on Boost.Beast: it reads each request, admits
 * it, finds its route in the router and answers what the route's handler
 * answers. It authenticates no one: the gateway in front of it does.
 */
class http_server final {
public:
    /**
     * @brief Binds the address. The router and the admission must outlive the
     * server.
     * @throws std::runtime_error when the address cannot be bound.
     */
    http_server(const net::router& r, admission admit, const server_options& o);
    ~http_server();
    http_server(const http_server&) = delete;
    http_server& operator=(const http_server&) = delete;

    /// The port the server listens on.
    [[nodiscard]] std::uint16_t port() const;

    /// Serves on the options' threads until stop() is called.
    void run();

    /// Makes run() return once the requests in flight are answered.
    void stop();

    /// What the server runs on, opaque outside its translation unit.
    struct state;

private:
    std::unique_ptr<state> state_;
};

}

#endif
