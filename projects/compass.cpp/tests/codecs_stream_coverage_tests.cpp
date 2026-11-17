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
#include "compass.cpp/codecs/stream_coverage.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.a/include/ores.a/subjects.hpp",
         "constexpr std::string_view book_saved = \"a.book.saved\";\n"
         "struct load_request {\n"
         "    static constexpr std::string_view nats_subject = \"a.load\";\n"
         "};\n"},
        {"projects/ores.a/src/service.cpp",
         "// ensure_stream(nats.make_stream_name(\"commented\"), x);\n"
         "void start(client& nats) {\n"
         "    nats.ensure_stream(nats.make_stream_name(\"a_events\"),\n"
         "        nats.covering_subjects({book_saved, \"a.book.deleted\"}));\n"
         "    nats.ensure_stream(nats.make_stream_name(\"wide\"),\n"
         "        nats.covering_subjects({\"a.book\"}));\n"
         "    nats.js_queue_subscribe(book_saved, \"d\", \"g\", h,\n"
         "        nats.make_stream_name(\"a_events\"), true, 1);\n"
         "    nats.js_queue_subscribe(\"a.orphan\", \"d\", \"g\", h,\n"
         "        nats.make_stream_name(\"a_events\"), true, 1);\n"
         "    nats.js_queue_subscribe(mystery, \"d\", \"g\", h,\n"
         "        nats.make_stream_name(\"a_events\"), true, 1);\n"
         "}\n"},
        {"projects/ores.b/src/steps.cpp",
         "void run(client& nats) {\n"
         "    step_routes routes{\"workers\"};\n"
         "    routes.on(load_request::nats_subject, handle);\n"
         "    serve_step_commands(nats, \"ores.b.service\", routes);\n"
         "}\n"},
        {"projects/ores.b/tests/t.cpp",
         "void t(client& nats) { nats.js_queue_subscribe(\"x\"); }\n"}};
}

}

TEST_CASE("each declaration the server would refuse or the check cannot "
    "read is reported", "[codecs][stream_coverage]") {
    const auto r = tests::run(codecs::stream_coverage, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.a/src/service.cpp", 11},
        {"projects/ores.a/src/service.cpp", 5},
        {"projects/ores.a/src/service.cpp", 7},
        {"projects/ores.a/src/service.cpp", 9}});
}

TEST_CASE("a service named by a parameter is resolved through a caller "
    "that names no stream, before and after an edit to the caller",
    "[codecs][stream_coverage]") {
    const files before{
        {"projects/ores.c/src/steps.cpp",
         "void\nserve(client& nats, std::string_view service) {\n"
         "    step_routes routes{\"w\"};\n"
         "    routes.on(\"c.load\", handle);\n"
         "    serve_step_commands(nats, service, routes);\n"
         "}\n"},
        {"projects/ores.c/src/main.cpp",
         "int main() { serve(nats, \"ores.c.service\"); }\n"}};
    auto after = before;
    after["projects/ores.c/src/main.cpp"] =
        "int main() { serve(nats, \"ores.c.other\"); }\n";

    CHECK(tests::run(codecs::stream_coverage, before).findings.empty());
    CHECK(tests::run_after_edit(codecs::stream_coverage, before, after)
        .findings.empty());

    after.erase("projects/ores.c/src/main.cpp");
    CHECK(tests::places(tests::run_after_edit(codecs::stream_coverage,
        before, after).findings) == places{
            {"projects/ores.c/src/steps.cpp", 5}, {"projects", 0}});
}
