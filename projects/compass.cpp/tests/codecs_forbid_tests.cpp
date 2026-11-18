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
#include "compass.cpp/codecs/forbid.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include "compass.cpp/ops/forbid.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
namespace ct = compass::codec::types;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

constexpr std::string_view portable_cpp =
    "FILES projects/**/*.{cpp,cc,cxx,hpp,h,hxx,ipp,inc}\n"
    "OUTSIDE projects/ores.platform/** projects/**/build/**\n"
    "\n"
    "RULE stop_types\n"
    "CPP_NAME std::jthread std::stop_token std::stop_source "
    "std::stop_callback\n"
    "CPP_INCLUDE <stop_token>\n"
    "MESSAGE {match} is not portable: Apple's libc++ has no C++20 stop "
    "types\n"
    "FIX use the same name in ores::platform::concurrency\n"
    "RULE posix_headers\n"
    "CPP_INCLUDE <unistd.h> <sys/time.h> <dirent.h>\n"
    "MESSAGE {match} is a POSIX header, which MSVC lacks\n"
    "RULE deprecated_c_calls\n"
    "CPP_NAME getenv std::getenv sscanf std::sscanf\n"
    "MESSAGE {match} is deprecated under MSVC\n"
    "FIX use ores::platform::environment for {match}\n"
    "RULE atomic_shared_ptr\n"
    "CPP_TEXT \\batomic\\s*<\\s*(std\\s*::\\s*)?shared_ptr\\b\n"
    "MESSAGE std::atomic<std::shared_ptr> is not portable\n";

constexpr std::array<ct::option, 1> options{{
    {codecs::forbid_definition_key, portable_cpp, "the definition"}}};

constexpr std::array<ct::rule, 4> rules{{
    {"stop_types", "stop types"}, {"posix_headers", "POSIX headers"},
    {"deprecated_c_calls", "deprecated C calls"},
    {"atomic_shared_ptr", "atomic shared_ptr"}}};

const ct::descriptor check{
    .name = "portable_cpp",
    .version = 1,
    .summary = "Reports each use of a facility a platform lacks.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = codecs::forbid_function()};

std::string error_of(std::string_view text) {
    const auto d = ops::parse_forbid_definition(text);
    return d ? std::string("parsed") : d.error();
}

}

TEST_CASE("the portable C++ definition parses into its globs and rules",
    "[ops][forbid]") {
    const auto d = ops::parse_forbid_definition(portable_cpp);
    REQUIRE(d);
    CHECK(d->files.size() == 8);
    CHECK(d->files.front() == "projects/**/*.cpp");
    CHECK(d->outside == std::vector<std::string>{
        "projects/ores.platform/**/*", "projects/**/build/**/*"});
    REQUIRE(d->rules.size() == 4);
    CHECK(d->rules[0].cpp_names.size() == 4);
    CHECK(d->rules[3].cpp_text ==
        R"(\batomic\s*<\s*(std\s*::\s*)?shared_ptr\b)");
}

TEST_CASE("a definition that cannot run says where and why",
    "[ops][forbid]") {
    CHECK(error_of("RULE r\nTEXT x\nMESSAGE m\n") ==
        "the definition has no FILES");
    CHECK(error_of("FILES *.cpp\n") == "the definition has no RULE");
    CHECK(error_of("FILES *.cpp\nRULE r\nMESSAGE m\n") ==
        "line 2: rule r forbids nothing; give it CPP_NAME, CPP_INCLUDE, "
        "CPP_TEXT or TEXT");
    CHECK(error_of("FILES *.cpp\nRULE r\nTEXT x\n") ==
        "line 2: rule r has no MESSAGE");
    CHECK(error_of("FILES *.cpp\nRULE r\nTEXT x\nMESSAGE m\nCOLOUR red\n")
        == "line 5: unknown key COLOUR");
    CHECK(error_of("FILES *.cpp\nRULE r\nTEXT x\nTEXT y\nMESSAGE m\n") ==
        "line 4: TEXT is given twice");
    CHECK(error_of("FILES a/{b,{c}}.cpp\n").starts_with(
        "line 1: nested { in"));
    CHECK(error_of("FILES a/{b.cpp\n").starts_with("line 1: unclosed { in"));
    CHECK(error_of("FILES *.cpp\nRULE r\nTEXT (\nMESSAGE m\n").starts_with(
        "line 3: regex ("));
    CHECK(error_of("FILES *.cpp\nRULE r\nTEXT x\nMESSAGE m\nRULE r\n") ==
        "line 5: rule r is defined twice");
    CHECK(error_of("FILES *.cpp\nRULE r\nTEXT x\nMESSAGE m\nFILES *.h\n") ==
        "line 5: FILES comes before the first RULE");
}

TEST_CASE("each brace in a glob stands for each of its alternatives",
    "[ops][forbid]") {
    const auto d = ops::parse_forbid_definition(
        "FILES {a,b}/x.{h,cpp}\nRULE r\nTEXT x\nMESSAGE m\n");
    REQUIRE(d);
    CHECK(d->files == std::vector<std::string>{
        "a/x.h", "a/x.cpp", "b/x.h", "b/x.cpp"});
}

TEST_CASE("the examples of the portable C++ check fire and stay quiet as "
    "the design says", "[codecs][forbid]") {
    const files t{
        {"projects/ores.trading/core/src/worker.cpp",
         "std::jthread worker([](std::stop_token t) { run(t); });\n"},
        {"projects/ores.trading/core/src/quiet.cpp",
         "ores::platform::concurrency::stoppable_thread worker(run);\n"},
        {"projects/ores.platform/src/concurrency/stoppable_thread.cpp",
         "#include <stop_token>\n"},
        {"projects/ores.http/core/src/server.cpp",
         "const char* port = std::getenv(\"ORES_PORT\");\n"},
        {"projects/ores.compute/core/src/cache.hpp",
         "std::atomic<std::shared_ptr<snapshot>> current_;\n"}};
    const auto r = tests::run(check, t);
    CHECK(r.findings == std::vector<tests::finding>{
        {"projects/ores.compute/core/src/cache.hpp", 1,
         "std::atomic<std::shared_ptr> is not portable"},
        {"projects/ores.http/core/src/server.cpp", 1,
         "std::getenv is deprecated under MSVC"},
        {"projects/ores.trading/core/src/worker.cpp", 1,
         "std::jthread is not portable: Apple's libc++ has no C++20 stop "
         "types"}});
    CHECK(r.fixes[1] == "use ores::platform::environment for std::getenv");
}

TEST_CASE("the stop types rule finds what portable_stop finds",
    "[codecs][forbid]") {
    const files t{
        {"projects/ores.comms/src/worker.cpp",
         "// std::jthread in a comment is not code\n"
         "const char* s = \"std::stop_token\";\n"
         "std::jthread worker;\n"
         "#include <stop_token>\n"
         "auto r = R\"x(std::stop_source)x\";\n"
         "void f(std::stop_callback<int> c);\n"},
        {"projects/ores.comms/include/ores.comms/a.hxx",
         "  #  include <stop_token>\n"},
        {"projects/ores.platform/include/stoppable_thread.hpp",
         "std::jthread allowed;\n"},
        {"projects/ores.comms/build/gen.cpp", "std::jthread skipped;\n"},
        {"projects/ores.comms/src/notes.txt", "std::jthread\n"},
        {"other/x.cpp", "std::jthread outside;\n"},
        {"projects/ores.a/src/a.cpp",
         "char q = '\"'; std::jthread a; auto s = \"x\";\n"
         "std :: stop_source b;\n"
         "#if 0\nstd::jthread dead;\n#include <stop_token>\n#endif\n"}};
    CHECK(tests::places(tests::run(check, t).findings) == places{
        {"projects/ores.a/src/a.cpp", 1},
        {"projects/ores.a/src/a.cpp", 2},
        {"projects/ores.comms/include/ores.comms/a.hxx", 1},
        {"projects/ores.comms/src/worker.cpp", 3},
        {"projects/ores.comms/src/worker.cpp", 4},
        {"projects/ores.comms/src/worker.cpp", 6}});
}

TEST_CASE("a name matches by its leading components and a member is no "
    "match", "[codecs][forbid]") {
    const files t{{"projects/ores.a/src/a.cpp",
        "auto a = getenv(\"A\");\n"
        "auto b = ::getenv(\"B\");\n"
        "auto c = ores::platform::environment::getenv(\"C\");\n"
        "auto d = env.getenv(\"D\");\n"
        "auto e = env->getenv(\"E\");\n"
        "std::jthread::id f;\n"
        "#include <unistd.h>\n"
        "#include \"unistd.h\"\n"
        "std::atomic < shared_ptr<int> > g;\n"
        "// std::atomic<std::shared_ptr<int>> h;\n"
        "#if 0\nstd::atomic<std::shared_ptr<int>> i;\n#endif\n"}};
    CHECK(tests::places(tests::run(check, t).findings) == places{
        {"projects/ores.a/src/a.cpp", 1}, {"projects/ores.a/src/a.cpp", 2},
        {"projects/ores.a/src/a.cpp", 6}, {"projects/ores.a/src/a.cpp", 7},
        {"projects/ores.a/src/a.cpp", 9}});
}

TEST_CASE("two rules on one line each report it, and one rule reports a "
    "line once", "[codecs][forbid]") {
    const files t{{"projects/ores.a/src/a.cpp",
        "std::jthread a; std::stop_token b; auto c = getenv(\"C\");\n"}};
    CHECK(tests::run(check, t).findings == std::vector<tests::finding>{
        {"projects/ores.a/src/a.cpp", 1,
         "std::jthread is not portable: Apple's libc++ has no C++20 stop "
         "types"},
        {"projects/ores.a/src/a.cpp", 1, "getenv is deprecated under MSVC"}});
}

TEST_CASE("after an edit only the edited file is scanned again",
    "[codecs][forbid]") {
    const files before{
        {"projects/ores.a/src/a.cpp", "std::jthread a;\n"},
        {"projects/ores.b/src/b.cpp", "int b;\n"}};
    auto after = before;
    after["projects/ores.b/src/b.cpp"] = "int b;\nauto e = getenv(\"E\");\n";

    const auto r = tests::run_after_edit(check, before, after);

    CHECK(tests::places(r.findings) == places{
        {"projects/ores.a/src/a.cpp", 1}, {"projects/ores.b/src/b.cpp", 2}});
}

namespace {

constexpr std::string_view with_examples =
    "SOURCE compass/checks/portable_cpp.org\n"
    "FILES src/**/*.cpp\n"
    "OUTSIDE src/platform/**\n"
    "RULE getenv\n"
    "CPP_NAME getenv std::getenv\n"
    "MESSAGE {match} is deprecated\n"
    "EXAMPLE fires=getenv src/a.cpp\n"
    "> auto p = std::getenv(\"P\");\n"
    "EXAMPLE quiet src/a.cpp\n"
    "> auto p = env.getenv(\"P\");\n"
    "EXAMPLE quiet src/platform/env.cpp\n"
    "> auto p = std::getenv(\"P\");\n"
    "EXAMPLE fires=getenv src/b.cpp\n"
    "> auto p = platform::read(\"P\");\n"
    "EXAMPLE quiet src/c.cpp\n"
    "> // getenv in a comment\n"
    ">\n"
    "> auto p = getenv(\"P\");\n"
    "EXAMPLE fires=getenv src/platform/d.cpp\n"
    "> auto p = getenv(\"P\");\n";

/**
 * @brief A check document whose Examples hold six :compass blocks, the
 * nth on line n + 3, after one that is not an example.
 */
constexpr std::string_view examples_document =
    "* Rule\n"
    "#+begin_src cpp :compass quiet\n"
    "* Examples\n"
    "#+begin_src cpp :path x.cpp\n"
    "#+begin_src cpp :compass fires=getenv\n"
    "#+begin_src cpp :compass quiet\n"
    "#+begin_src cpp :compass quiet\n"
    "#+begin_src cpp :compass fires=getenv\n"
    "  #+BEGIN_SRC cpp :compass quiet\n"
    "#+begin_src cpp :compass fires=getenv\n";

std::uint32_t offset_of_block(std::size_t n) {
    std::size_t at = examples_document.find("* Examples");
    for (std::size_t i = 0; i <= n; ++i)
        at = examples_document.find(":compass", at + 1);
    return static_cast<std::uint32_t>(
        examples_document.rfind('\n', at) + 1);
}

std::vector<std::tuple<std::uint32_t, std::uint32_t, std::string,
    std::string>> example_findings(std::string_view text) {
    const auto d = ops::parse_forbid_definition(text);
    REQUIRE(d);
    ct::diagnostic_table out;
    ops::check_examples(*d, examples_document, out);
    std::vector<std::tuple<std::uint32_t, std::uint32_t, std::string,
        std::string>> r;
    for (std::size_t i = 0; i < out.size(); ++i) {
        const ct::diagnostic_id id{static_cast<std::uint32_t>(i)};
        CHECK(out.files[id] == "compass/checks/portable_cpp.org");
        r.emplace_back(out.ranges[id].begin, out.rules[id].value,
            std::string(out.messages[id]), std::string(out.fixes[id]));
    }
    return r;
}

}

TEST_CASE("an example that does not do what it says is reported where it "
    "is written", "[ops][forbid]") {
    const auto r = example_findings(with_examples);
    REQUIRE(r.size() == 3);
    CHECK(r[0] == std::tuple{offset_of_block(3), 1u,
        std::string("the example of src/b.cpp raises no getenv finding"),
        std::string("widen the rule so the example fires, or correct the "
            "example")});
    CHECK(std::get<0>(r[1]) == offset_of_block(4));
    CHECK(std::get<2>(r[1]) == "the quiet example of src/c.cpp raises getenv");
    CHECK(std::get<0>(r[2]) == offset_of_block(5));
    CHECK(std::get<3>(r[2]) == "the definition does not cover "
        "src/platform/d.cpp; correct its FILES or OUTSIDE, or the example's "
        "path");
}

TEST_CASE("an example's body keeps its lines, blank ones too",
    "[ops][forbid]") {
    const auto d = ops::parse_forbid_definition(with_examples);
    REQUIRE(d);
    REQUIRE(d->examples.size() == 6);
    CHECK(d->examples[4].body ==
        "// getenv in a comment\n\nauto p = getenv(\"P\");\n");
    CHECK(d->examples[0].fires == "getenv");
    CHECK(d->examples[1].fires.empty());
    CHECK(d->source == "compass/checks/portable_cpp.org");
}

TEST_CASE("an example that cannot be read says where", "[ops][forbid]") {
    CHECK(error_of("FILES *.cpp\nRULE r\nTEXT x\nMESSAGE m\n"
        "EXAMPLE fires=s a.cpp\n") ==
        "line 5: the example fires s, which is no rule of the definition");
    CHECK(error_of("FILES *.cpp\nRULE r\nTEXT x\nMESSAGE m\n"
        "EXAMPLE loud a.cpp\n") ==
        "line 5: EXAMPLE takes quiet or fires=<rule> and a path");
    CHECK(error_of("FILES *.cpp\nRULE r\nTEXT x\nMESSAGE m\n"
        "EXAMPLE quiet a.cpp b.cpp\n") ==
        "line 5: EXAMPLE takes quiet or fires=<rule> and a path");
}
