#!/usr/bin/env python3
#
# Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
"""Writes the archetype pages of masd.cpp.http-service from compass.serve's copy.

usage: http_service_family.py <checkout> <ids.tsv>

The copy of ORE Studio's route machinery was first adapted in place, under
projects/compass.serve, and compiled there, beside compass's own binding
runtime. This turns each file into an archetype page of share/masd/templates
whose template is the file with the component's namespace, include root and
guard prefix, and the JSON codec runtime's namespace and include root, made
parameters, so a render for compass.serve gives back the file. ids.tsv holds
one archetype name and its :ID: per line, made with uuidgen.
"""
import pathlib
import sys
import textwrap

ORE_COMMIT = "bc0702ad46b8a6097ce5242ece7ba01067699f60"
FACET_ID = "58BCC011-F984-4DAA-B054-F3692C529F38"
ORE_ROOT = "projects/ores.http/api/"
VERBATIM = "Verbatim but for the parameters."
NO_EXPORT = "Verbatim but for the parameters and the export macro, which compass's static library does not need."

# name, rendered file, ORE Studio file, what it holds, how it differs
FILES = [
    ("http_method", "include/compass.serve/domain/http_method.hpp",
     "include/ores.http.api/domain/http_method.hpp",
     "The HTTP methods a route answers.", VERBATIM),
    ("http_status", "include/compass.serve/domain/http_status.hpp",
     "include/ores.http.api/domain/http_status.hpp",
     "The HTTP statuses a response carries.", VERBATIM),
    ("body_sink", "include/compass.serve/domain/body_sink.hpp",
     "include/ores.http.api/domain/body_sink.hpp",
     "body_sink, where a route that streams its body writes it.", NO_EXPORT),
    ("body_source", "include/compass.serve/domain/body_source.hpp",
     "include/ores.http.api/domain/body_source.hpp",
     "body_source, a response body read as it is written.", NO_EXPORT),
    ("awaitable_body_source",
     "include/compass.serve/domain/awaitable_body_source.hpp",
     "include/ores.http.api/domain/awaitable_body_source.hpp",
     "awaitable_body_source, a response body whose source may wait.",
     NO_EXPORT),
    ("http_request", "include/compass.serve/domain/http_request.hpp",
     "include/ores.http.api/domain/http_request.hpp",
     "http_request, a request as a route's handler sees it.",
     "The JWT claims and the bearer token are left out: compass reads its "
     "caller from the gateway's headers, never from a token."),
    ("http_request_impl", "src/domain/http_request.cpp",
     "src/domain/http_request.cpp",
     "The definitions of http_request's lookups.",
     "get_bearer_token is left out, as its declaration is."),
    ("http_response", "include/compass.serve/domain/http_response.hpp",
     "include/ores.http.api/domain/http_response.hpp",
     "http_response and its constructors, api_error among them.", NO_EXPORT),
    ("http_response_impl", "src/domain/http_response.cpp",
     "src/domain/http_response.cpp",
     "The definitions of http_response's constructors, which write their "
     "bodies through the codec runtime.", VERBATIM),
    ("route", "include/compass.serve/domain/route.hpp",
     "include/ores.http.api/domain/route.hpp",
     "route and route_catalogue: a route's pattern, handler, catalogue place, "
     "parameters and schemas.",
     "The authentication position, requires_auth, auth_declared and "
     "required_roles, is caller_need caller and caller_declared: what the "
     "gateway must present. input_schema is added, the JSON Schema of "
     "everything the route's tool takes. No export macro."),
    ("route_impl", "src/domain/route.cpp", "src/domain/route.cpp",
     "route::match, which fills the path parameters, percent-decoded.",
     VERBATIM),
    ("catalogue_rules", "include/compass.serve/domain/catalogue_rules.hpp",
     "include/ores.http.api/domain/catalogue_rules.hpp",
     "The rules of the catalogue: the published areas and which names "
     "only read.",
     "area_of_component, entity_route_catalogue and operation_route_catalogue "
     "are left out: they place ORE Studio's generated entity and NATS "
     "operation routes, and every compass route states its place."),
    ("catalogue_rules_impl", "src/domain/catalogue_rules.cpp",
     "src/domain/catalogue_rules.cpp",
     "The published areas and the rules over them.",
     "The published areas are compass's three, agile, documents and qa, "
     "where ORE Studio's are sixteen. The three functions the header leaves "
     "out are left out."),
    ("router", "include/compass.serve/net/router.hpp",
     "include/ores.http.api/net/router.hpp",
     "route_builder and router: the fluent declaration of a route, its "
     "matching, and the start-up gates of uncatalogued().",
     "auth_required(), auth_optional() and roles() are caller(caller_need). "
     "input<T>() is added, the tool's input schema from the request type. "
     "entity_catalogue and operation_catalogue are left out with the rules "
     "they called. The logger is left out: compass has none. rate_limit() is "
     "kept and not called: the gateway limits."),
    ("router_impl", "src/net/router.cpp", "src/net/router.cpp",
     "The definitions of route_builder and router, the gates among them.",
     "As the header: caller() for the authentication calls, and no log "
     "lines. The gates are verbatim."),
    ("endpoint_registry", "include/compass.serve/openapi/endpoint_registry.hpp",
     "include/ores.http.api/openapi/endpoint_registry.hpp",
     "endpoint_registry, the OpenAPI document of the registered routes.",
     "The session cookie's name is left out, and the default title names "
     "compass serve. No logger, no export macro."),
    ("endpoint_registry_impl", "src/openapi/endpoint_registry.cpp",
     "src/openapi/endpoint_registry.cpp",
     "The OpenAPI document: each route's operationId, catalogue place, "
     "parameters, body and response, with schemas lifted into components.",
     "The tags are compass's three areas. The security schemes are the "
     "gateway's Compass-View and Compass-Subject headers, where ORE Studio's "
     "are a bearer token and a session cookie. The catalogue extensions are "
     "x-compass-area, x-compass-entity and x-compass-writes, where ORE "
     "Studio's are x-ores-. No log lines."),
    ("caller", "include/compass.serve/domain/caller.hpp", None,
     "caller: the view, the subject and the actor the gateway names.",
     "compass's own: ORE Studio's caller is its JWT claims."),
    ("refusal", "include/compass.serve/domain/refusal.hpp", None,
     "refusal: a status, a code from the service's closed list and a "
     "sentence, which every route answers as api_error writes it.",
     "compass's own."),
    ("binding", "include/compass.serve/net/binding.hpp", None,
     "binding: a request message from the path, the query and the body by "
     "the service's binding rules, and body_schema, a request's schema "
     "narrowed to its body.",
     "compass's own: ORE Studio's operation routes take one JSON body and "
     "bind nothing from the path or the query."),
    ("binding_impl", "src/net/binding.cpp", None,
     "The query string's parameters, the body's checks and body_schema.",
     "compass's own."),
    ("answer", "include/compass.serve/net/answer.hpp", None,
     "answer: the caller a route needs, then its call, written as JSON or "
     "as the refusal; and ready, the answer as a handler's awaitable.",
     "compass's own: ORE Studio's routes forward over NATS."),
    ("answer_impl", "src/net/answer.cpp", None,
     "caller_of, the caller from the gateway's headers.", "compass's own."),
]

PARAMETERS = [
    ("compass::db::codec", "<%wire_namespace%>"),
    ("compass.db/codec/", "<%wire_root%>/"),
    ("compass::serve", "<%component_namespace%>"),
    ("compass.serve/", "<%component_root%>/"),
    ("COMPASS_SERVE_", "<%guard_prefix%>"),
]

LICENCE_LINES = 19


def fill(text):
    return textwrap.fill(text, 76, break_long_words=False,
                         break_on_hyphens=False)


def template_of(text):
    body = "".join(text.splitlines(keepends=True)[LICENCE_LINES:])
    if "<%" in body or "%>" in body:
        raise SystemExit("a file already holds a delimiter <% or %>")
    for old, new in PARAMETERS:
        body = body.replace(old, new)
    return body


def page(name, ident, rendered, ore, holds, differs, text):
    stem = f"masd.cpp.http-service.{name}"
    ext = "cpp" if rendered.endswith(".cpp") else "hpp"
    tangle = f"masd_cpp_http_service_{name}.{ext}.mustache"
    output = ("projects/{component_dir}/" +
              rendered.replace("compass.serve/", "{component_dir}/"))
    origin = (f"ORE Studio's ={ORE_ROOT}{ore}= at commit ={ORE_COMMIT}=."
              if ore else "No ORE Studio file: compass's own.")
    return f""":PROPERTIES:
:ID: {ident}
:END:
#+title: {stem}
#+description: {holds.replace('=', '')} Output {output}.
#+type: archetype
#+output: {output}
#+facet: masd.cpp.http-service
#+level: cross
#+filetags: :codegen:templates:literate:archetype:masd:
#+created: 2026-11-23
#+updated: 2026-11-23

This page is an archetype of the [[id:{FACET_ID}][masd.cpp.http-service]] facet:
one file of the HTTP route runtime, rendered into the component that switches
the facet on.

* Origin

{fill(origin)}

{fill(differs)}

* Context

The context is the library's values and =masd_component=:
=component_namespace=, =component_root=, the directory the component's
headers are included from, =guard_prefix=, and =wire_namespace= and
=wire_root=, the JSON codec runtime's namespace and include directory. The
template switches the delimiters to =<% %>=, because the C++ it holds has
braces of its own.

* Template

The template source. compass's =tangle= codec writes it to
={tangle}= beside this page.

#+begin_src mustache :tangle {tangle}
{{{{! GENERATED FILE — tangled from share/masd/templates/{stem}.org. Edit the org source. }}}}
{{{{=<% %>=}}}}
<%& cpp_license%>
<%#masd_component%>
{text}<%/masd_component%>
#+end_src

* See also

- Parent facet: [[id:{FACET_ID}][masd.cpp.http-service]]
"""


def main():
    root = pathlib.Path(sys.argv[1])
    ids = dict(line.split("\t") for line in
               pathlib.Path(sys.argv[2]).read_text().split("\n") if line)
    out = root / "share/masd/templates"
    for name, rendered, ore, holds, differs in FILES:
        text = (root / "projects/compass.serve" / rendered).read_text()
        (out / f"masd.cpp.http-service.{name}.org").write_text(
            page(name, ids[name], rendered, ore, holds, differs,
                 template_of(text)))


main()
