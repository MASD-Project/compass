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
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "load_fixture.hpp"

namespace {

using compass::model::tests::checkout;
namespace types = compass::model::types;

constexpr std::string_view operation_path =
    "projects/compass.serve/modeling/compass.serve.notes.org";

constexpr std::string_view component_path =
    "projects/compass.db/modeling/compass.db.component.org";

/**
 * @brief An operation model with one routed request, whose environment
 * field states @p environment and whose reply field states @p reply, and
 * one message no route takes, whose field states @p plain.
 */
std::string operation(std::string_view header, std::string_view request,
    std::string_view environment, std::string_view reply,
    std::string_view plain = ":cpp_type: std::string\n") {
    return std::string(R"org(#+title: compass.serve.notes
#+type: masd.codegen.operation
#+component: serve
#+entity_singular: notes
)org") + std::string(header) + R"org(
* Includes
#+begin_src cpp :name includes
#include <optional>
#include <string>
#+end_src

* Messages

** reply_request
:PROPERTIES:
:response:    reply_response
)org" + std::string(request) + R"org(:END:

*** environment
:PROPERTIES:
)org" + std::string(environment) + R"org(:END:

*** reply
:PROPERTIES:
)org" + std::string(reply) + R"org(:END:

*** limit
:PROPERTIES:
:cpp_type:  std::uint32_t
:default:   20
:http_in:   query
:http_name: max
:END:

** reply_response
*** text
:PROPERTIES:
)org" + std::string(plain) + R"org(:END:
)org";
}

constexpr std::string_view namespace_line =
    "#+namespace: compass::serve::messages\n";
constexpr std::string_view routed =
    ":http_method: post\n:http_path:   /v1/{environment}/reply\n";
constexpr std::string_view path_field =
    ":cpp_type: std::string\n:http_in:  path\n";
constexpr std::string_view body_field =
    ":cpp_type: std::string\n:http_in:  body\n:required: false\n";

checkout with(std::string source) {
    checkout c;
    c.add(std::string(operation_path), std::move(source));
    return c;
}

void refused(std::string source, std::string_view says) {
    const auto error = with(std::move(source)).load_error(operation_path);
    INFO(error);
    CHECK(error.find(says) != std::string::npos);
}

std::string component(std::string_view flags) {
    return std::string(R"org(#+title: compass.db.component
#+type: masd.codegen.component

* Flags
:PROPERTIES:
)org") + std::string(flags) + ":END:\n";
}

}

TEST_CASE("a masd.codegen.operation model reads its messages and where a "
    "route reads each field", "[ops][load][masd]") {
    const auto c = with(operation(namespace_line, routed, path_field,
        body_field));
    REQUIRE(c.load_error(operation_path).empty());
    const auto m = c.masd_operation(operation_path);
    CHECK(m.component == "serve");
    CHECK(m.entity == "notes");
    CHECK(m.ns == "compass::serve::messages");
    CHECK(m.includes == std::vector<std::string>{"<optional>", "<string>"});
    REQUIRE(m.messages.size() == 2);
    const auto& request = m.messages[0];
    CHECK(request.response == "reply_response");
    CHECK(request.http_method == "post");
    CHECK(request.http_path == "/v1/{environment}/reply");
    REQUIRE(request.fields.size() == 3);
    CHECK(request.fields[0].http_in == types::http_in::path);
    CHECK(request.fields[0].required);
    CHECK(request.fields[1].http_in == types::http_in::body);
    CHECK_FALSE(request.fields[1].required);
    CHECK(request.fields[2].http_in == types::http_in::query);
    CHECK(request.fields[2].default_value == "20");
    CHECK(request.fields[2].http_name == "max");
    const auto& response = m.messages[1];
    CHECK(response.http_method.empty());
    REQUIRE(response.fields.size() == 1);
    CHECK(response.fields[0].http_in == types::http_in::none);
}

TEST_CASE("an operation model that says what the format does not have is "
    "refused, naming where", "[ops][load][masd]") {
    SECTION("a misspelt field property") {
        refused(operation(namespace_line, routed, path_field,
            ":cpp_type: std::string\n:http_in: body\n:defualt: x\n"),
            "reply_request.reply has the property 'defualt'");
    }
    SECTION("a misspelt message property") {
        refused(operation(namespace_line,
            std::string(routed) + ":respones: x\n", path_field, body_field),
            "reply_request has the property 'respones'");
    }
    SECTION("a field with no type") {
        refused(operation(namespace_line, routed, ":http_in: path\n",
            body_field), "reply_request.environment states no :cpp_type:");
    }
    SECTION("an encoding other than base64") {
        refused(operation(namespace_line, routed, path_field,
            std::string(body_field) + ":encoding: hex\n"),
            "reply_request.reply's :encoding: is 'hex'; it is base64");
    }
    SECTION("a place a route cannot read") {
        refused(operation(namespace_line, routed,
            ":cpp_type: std::string\n:http_in: header\n", body_field),
            ":http_in: is 'header'; it is path, query or body");
    }
    SECTION("a required other than false") {
        refused(operation(namespace_line, routed, path_field,
            ":cpp_type: std::string\n:http_in: body\n:required: true\n"),
            "reply_request.reply's :required: is 'true'");
    }
    SECTION("a routed request's field that names no place") {
        refused(operation(namespace_line, routed, ":cpp_type: std::string\n",
            body_field), "reply_request.environment states no :http_in:");
    }
    SECTION("a place on a message no route takes") {
        refused(operation(namespace_line, routed, path_field, body_field,
            ":cpp_type: std::string\n:http_in: body\n"),
            "reply_response.text states :http_in: on a message no route "
            "takes");
    }
    SECTION("a path field that may be left out") {
        refused(operation(namespace_line, routed,
            std::string(path_field) + ":required: false\n", body_field),
            "reply_request.environment is a path field");
    }
    SECTION("a body on a GET") {
        refused(operation(namespace_line,
            ":http_method: get\n:http_path: /v1/{environment}/reply\n",
            path_field, body_field),
            "reply_request.reply is a body field of a GET");
    }
    SECTION("a method HTTP does not have") {
        refused(operation(namespace_line,
            ":http_method: fetch\n:http_path: /v1/x\n", path_field,
            body_field), ":http_method: is 'fetch'");
    }
    SECTION("a method without a path") {
        refused(operation(namespace_line, ":http_method: post\n",
            path_field, body_field),
            "states one of :http_method: and :http_path: without the other");
    }
    SECTION("no namespace") {
        refused(operation("", routed, path_field, body_field),
            "an operation model states #+component:, #+entity_singular: "
            "and #+namespace:");
    }
}

TEST_CASE("a masd.codegen.component model reads its product and component",
    "[ops][load][masd]") {
    checkout c;
    c.add(std::string(component_path),
        component(":product:   compass\n:component: db\n"));
    REQUIRE(c.load_error(component_path).empty());
    const auto m = c.masd_component(component_path);
    CHECK(m.product == "compass");
    CHECK(m.component == "db");
}

TEST_CASE("a component model that leaves out its product or states another "
    "flag is refused", "[ops][load][masd]") {
    SECTION("no product") {
        checkout c;
        c.add(std::string(component_path), component(":component: db\n"));
        CHECK(c.load_error(component_path).find(
            "a component model states its product and component") !=
            std::string::npos);
    }
    SECTION("a flag the format does not have") {
        checkout c;
        c.add(std::string(component_path),
            component(":product: compass\n:component: db\n:tenant: x\n"));
        CHECK(c.load_error(component_path).find(
            "* Flags has the property 'tenant'") != std::string::npos);
    }
}
