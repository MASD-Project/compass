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
/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Template: masd_cpp_http_service_answer.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_NET_ANSWER_HPP
#define COMPASS_SERVE_NET_ANSWER_HPP

#include "compass.serve/domain/caller.hpp"
#include "compass.serve/domain/http_request.hpp"
#include "compass.serve/domain/http_response.hpp"
#include "compass.serve/domain/refusal.hpp"
#include "compass.serve/domain/route.hpp"
#include "compass.db/codec/wire_traits_fwd.hpp"
#include <boost/asio/awaitable.hpp>
#include <exception>
#include <type_traits>

namespace compass::serve::net {

/**
 * @brief The caller the gateway names in the request's headers.
 *
 * @throws domain::refusal no_view when there is no Compass-View, and
 * no_subject when @p need is a subject and there is no Compass-Subject.
 */
[[nodiscard]] domain::caller caller_of(const domain::http_request& request,
                                       domain::caller_need need);

/**
 * @brief @p response as a route handler's result: an awaitable that holds
 * the answer already made, so nothing it needs outlives the call.
 */
inline boost::asio::awaitable<domain::http_response> ready(domain::http_response response) {
    co_return response;
}

/**
 * @brief The answer of a route: reads the caller its position needs, runs
 * @p call with it and writes the message it returns as JSON, 204 when it
 * returns nothing.
 *
 * A refusal, from the caller, the binding or the handler, answers its status
 * and its code; any other failure answers internal, naming no internals.
 */
template <typename Call>
domain::http_response
answer(const domain::http_request& request, domain::caller_need need, Call&& call) {
    try {
        const auto c = caller_of(request, need);
        using result = std::invoke_result_t<Call, const domain::caller&>;
        if constexpr (std::is_void_v<result>) {
            call(c);
            domain::http_response r;
            r.status = domain::http_status::no_content;
            return r;
        } else if constexpr (std::is_same_v<result, domain::http_response>) {
            return call(c);
        } else {
            return domain::http_response::json(
                compass::db::codec::wire_traits<result>::write_json(call(c)));
        }
    } catch (const domain::refusal& r) {
        return domain::http_response::api_error(r.status(), r.code(), r.what());
    } catch (const std::exception&) {
        return domain::http_response::api_error(domain::http_status::internal_server_error,
                                                "internal",
                                                "The service could not answer.");
    }
}

}

#endif
