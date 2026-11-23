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
#include "compass.serve/app/http_server.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include "compass.serve/net/binding.hpp"

namespace compass::serve::app {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

struct http_server::state {
    const net::router& router;
    admission admit;
    server_options options;
    asio::io_context io;
    tcp::acceptor acceptor;

    state(const net::router& r, admission a, const server_options& o)
        : router(r)
        , admit(std::move(a))
        , options(o)
        , io(static_cast<int>(std::max(1U, o.threads)))
        , acceptor(io) {}
};

namespace {

std::optional<domain::http_method> method_of(http::verb v) {
    switch (v) {
    case http::verb::get: return domain::http_method::get;
    case http::verb::post: return domain::http_method::post;
    case http::verb::put: return domain::http_method::put;
    case http::verb::patch: return domain::http_method::patch;
    case http::verb::delete_: return domain::http_method::delete_;
    case http::verb::head: return domain::http_method::head;
    case http::verb::options: return domain::http_method::options;
    default: return std::nullopt;
    }
}

http::response<http::string_body> to_beast(const domain::http_response& r, unsigned version,
                                           bool keep_alive) {
    http::response<http::string_body> out{static_cast<http::status>(r.status), version};
    out.set(http::field::server, "compass serve");
    if (!r.body.empty() || r.status != domain::http_status::no_content)
        out.set(http::field::content_type, r.content_type);
    for (const auto& [name, value] : r.headers)
        out.set(name, value);
    out.body() = r.body;
    out.keep_alive(keep_alive);
    out.prepare_payload();
    return out;
}

domain::http_response refused(domain::http_status status, std::string_view code,
                              std::string_view message) {
    return domain::http_response::api_error(status, std::string(code), std::string(message));
}

asio::awaitable<void> session(http_server::state& s, tcp::socket socket) {
    beast::tcp_stream stream(std::move(socket));
    beast::flat_buffer buffer;
    const auto remote = stream.socket().remote_endpoint().address().to_string();
    for (;;) {
        http::request_parser<http::string_body> parser;
        stream.expires_after(std::chrono::seconds(30));
        auto [read_header, header_bytes] = co_await http::async_read_header(
            stream, buffer, parser, asio::as_tuple(asio::use_awaitable));
        if (read_header)
            break;

        domain::http_request request;
        const auto& head = parser.get();
        request.target = std::string(head.target());
        request.http_version_major = static_cast<std::uint16_t>(head.version() / 10);
        request.http_version_minor = static_cast<std::uint16_t>(head.version() % 10);
        request.remote_address = remote;
        for (const auto& field : head) {
            std::string name(field.name_string());
            std::ranges::transform(name, name.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
            request.headers[name] = std::string(field.value());
        }
        const auto method = method_of(head.method());
        const auto path = request.target.substr(0, request.target.find('?'));
        std::optional<domain::route> route;
        if (method) {
            request.method = *method;
            route = s.router.match(*method, path, request.path_params);
        }
        parser.body_limit(route && route->max_body_size > 0 ? route->max_body_size
                                                            : s.options.max_body);

        domain::http_response response;
        auto [read_body, body_bytes] =
            co_await http::async_read(stream, buffer, parser, asio::as_tuple(asio::use_awaitable));
        const bool keep_alive = parser.get().keep_alive();
        if (read_body == http::error::body_limit) {
            response = refused(domain::http_status::payload_too_large, "too_large",
                               "The body is longer than this route reads.");
        } else if (read_body) {
            break;
        } else if (!route) {
            response = refused(domain::http_status::not_found, "not_found",
                               "No route answers " + std::string(head.method_string()) + " " +
                                   path + ".");
        } else {
            request.body = std::move(parser.get().body());
            request.body_bytes = request.body.size();
            for (auto& [name, value] : net::query_parameters(request.target))
                request.query_params.try_emplace(name, value);
            if (auto refusal = s.admit(request))
                response = std::move(*refusal);
            else
                response = co_await route->handler(request);
        }

        const auto out = to_beast(response, head.version(), keep_alive && !read_body);
        auto [wrote, written] =
            co_await http::async_write(stream, out, asio::as_tuple(asio::use_awaitable));
        if (wrote || !out.keep_alive())
            break;
    }
    beast::error_code ignored;
    stream.socket().shutdown(tcp::socket::shutdown_send, ignored);
}

asio::awaitable<void> listen(http_server::state& s) {
    for (;;) {
        auto [ec, socket] = co_await s.acceptor.async_accept(asio::as_tuple(asio::use_awaitable));
        if (ec) {
            if (ec == asio::error::operation_aborted)
                co_return;
            continue;
        }
        asio::co_spawn(s.io, session(s, std::move(socket)), asio::detached);
    }
}

}

http_server::http_server(const net::router& r, admission admit, const server_options& o)
    : state_(std::make_unique<state>(r, std::move(admit), o)) {
    beast::error_code ec;
    const auto address = asio::ip::make_address(o.address, ec);
    if (ec)
        throw std::runtime_error("compass serve: " + o.address + " is not an address");
    const tcp::endpoint endpoint(address, o.port);
    state_->acceptor.open(endpoint.protocol(), ec);
    if (!ec)
        state_->acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if (!ec)
        state_->acceptor.bind(endpoint, ec);
    if (!ec)
        state_->acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec)
        throw std::runtime_error("compass serve: cannot listen on " + o.address + ":" +
                                 std::to_string(o.port) + ": " + ec.message());
}

http_server::~http_server() = default;

std::uint16_t http_server::port() const {
    return state_->acceptor.local_endpoint().port();
}

void http_server::run() {
    asio::co_spawn(state_->io, listen(*state_), asio::detached);
    std::vector<std::thread> threads;
    for (unsigned i = 1; i < std::max(1U, state_->options.threads); ++i)
        threads.emplace_back([this] { state_->io.run(); });
    state_->io.run();
    for (auto& t : threads)
        t.join();
}

void http_server::stop() {
    asio::post(state_->io, [this] {
        beast::error_code ignored;
        state_->acceptor.close(ignored);
        state_->io.stop();
    });
}

}
