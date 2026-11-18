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
#include "compass.cpp/codecs/forward_declarable_includes.hpp"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::cpp;
namespace ct = compass::codec::types;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

constexpr std::string_view both_types =
    "ores::db::pq::context;class;ores.db/pq/context.hpp;"
    "ores.db/pq/context_fwd.hpp,"
    "ores::db::pool;struct;ores.db/pool.hpp;-";

constexpr std::string_view list_path =
    "build/scripts/forward_declarable_types.list";

/**
 * @brief The codec with its types option defaulting to @p types and its
 * list_file option to @p list_file.
 */
ct::descriptor listing(std::string_view types,
    std::string_view list_file = list_path) {
    static std::vector<std::unique_ptr<std::array<ct::option, 2>>> kept;
    auto d = codecs::forward_declarable_includes;
    kept.push_back(std::make_unique<std::array<ct::option, 2>>(
        std::array<ct::option, 2>{{{"types", types, ""},
            {"list_file", list_file, ""}}}));
    d.options = *kept.back();
    return d;
}

/**
 * @brief A list file of the two types of both_types, the pool's row
 * written as @p pool_row.
 */
std::string list_of(std::string_view pool_row) {
    return "# The forward-declarable types.\n"
        "\n"
        "ores::db::pq::context  class  ores.db/pq/context.hpp  "
        "ores.db/pq/context_fwd.hpp\n" + std::string(pool_row) + "\n";
}

files with_list(std::string_view pool_row) {
    auto out = files{{"projects/ores.db/include/ores.db/pq/context.hpp",
        "namespace ores::db::pq { class context { int x; }; }\n"}};
    out.emplace(list_path, list_of(pool_row));
    return out;
}

files tree() {
    return {
        {"projects/ores.db/include/ores.db/pq/context.hpp",
         "namespace ores::db::pq { class context { int x; }; }\n"},
        {"projects/ores.x/include/ores.x/pointer.hpp",
         "#pragma once\n"
         "#include \"ores.db/pq/context.hpp\"\n"
         "#include \"ores.db/pool.hpp\"\n"
         "void f(const ores::db::pq::context* c);\n"
         "void g(std::shared_ptr<db::pool> p);\n"},
        {"projects/ores.x/include/ores.x/value.hpp",
         "#include \"ores.db/pq/context.hpp\"\n"
         "struct holder { pq::context c; };\n"},
        {"projects/ores.x/include/ores.x/calls.hpp",
         "#include \"ores.db/pq/context.hpp\"\n"
         "inline int h(const pq::context& ctx) { return ctx.size(); }\n"},
        {"projects/ores.x/include/ores.x/comment.hpp",
         "#include \"ores.db/pq/context.hpp\"\n"
         "// pq::context* in a comment\n"},
        {"projects/ores.x/include/ores.x/literal.hpp",
         "#include \"ores.db/pq/context.hpp\"\n"
         "void f(pq::context* c);\n"
         "const char* s = \"pq::context value;\";\n"},
        {"projects/ores.x/include/ores.x/opened.hpp",
         "#include \"ores.db/pool.hpp\"\n"
         "namespace ores::db {\n"
         "class pool;\n"
         "void use(pool& p);\n"
         "}\n"},
        {"projects/ores.x/build/skipped.hpp",
         "#include \"ores.db/pool.hpp\"\nvoid g(db::pool* p);\n"}};
}

}

TEST_CASE("a header that includes a definition it only names is reported "
    "on the include", "[codecs][forward_declarable_includes]") {
    const auto r = tests::run(listing(both_types), tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.x/include/ores.x/literal.hpp", 1},
        {"projects/ores.x/include/ores.x/opened.hpp", 1},
        {"projects/ores.x/include/ores.x/pointer.hpp", 2},
        {"projects/ores.x/include/ores.x/pointer.hpp", 3}});
}

TEST_CASE("an empty or malformed type list is reported, as it turns the "
    "guard off", "[codecs][forward_declarable_includes]") {
    CHECK(tests::places(tests::run(listing(""), tree()).findings) ==
        places{{".compass.org", 0}});
    const auto r = tests::run(listing("ores::db::pool;union;a.hpp;-"),
        tree());
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<2>(r.findings[0]).find("must be class or struct") !=
        std::string::npos);
}

TEST_CASE("the types are an option, so a header is decided alone and an "
    "edit to one changes only its findings",
    "[codecs][forward_declarable_includes]") {
    CHECK(tests::places(tests::run(listing("ores::db::pool;struct;"
        "ores.db/pool.hpp;-"), tree()).findings) ==
        places{{"projects/ores.x/include/ores.x/opened.hpp", 1},
            {"projects/ores.x/include/ores.x/pointer.hpp", 3}});

    auto edited = tree();
    edited["projects/ores.x/include/ores.x/value.hpp"] =
        "#include \"ores.db/pq/context.hpp\"\n"
        "void f(pq::context* c);\n";
    CHECK(tests::places(tests::run_after_edit(listing(both_types), tree(),
        edited).findings) == places{
            {"projects/ores.x/include/ores.x/literal.hpp", 1},
            {"projects/ores.x/include/ores.x/opened.hpp", 1},
            {"projects/ores.x/include/ores.x/pointer.hpp", 2},
            {"projects/ores.x/include/ores.x/pointer.hpp", 3},
            {"projects/ores.x/include/ores.x/value.hpp", 1}});
}

TEST_CASE("ORE Studio's six types are the default",
    "[codecs][forward_declarable_includes]") {
    const auto r = tests::run(codecs::forward_declarable_includes, {
        {"projects/ores.a/include/ores.a/x.hpp",
         "#include <ores.database/pq/pool.hpp>\n"
         "void f(ores::database::pq::pool& p);\n"}});
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.a/include/ores.a/x.hpp", 1}});
}

TEST_CASE("the default defines the session context in its own header, so "
    "ORE Studio's unit_of_work.hpp, which takes its declaration and "
    "constructs a transaction in a function template, is not reported",
    "[codecs][forward_declarable_includes]") {
    const auto r = tests::run(codecs::forward_declarable_includes, {
        {"projects/ores.database/include/ores.database/pq/unit_of_work.hpp",
         "#include \"ores.database/pq/pool.hpp\"\n"
         "#include \"ores.database/pq/session_context_fwd.hpp\"\n"
         "#include \"ores.database/pq/transaction.hpp\"\n"
         "#include <type_traits>\n"
         "#include <utility>\n"
         "namespace ores::database::pq {\n"
         "template <typename Work>\n"
         "auto transact(pool& p, const session_context& context, "
         "Work&& work)\n"
         "    -> std::invoke_result_t<Work, transaction&> {\n"
         "    auto lease = p.acquire();\n"
         "    transaction tx(lease.get(), context);\n"
         "    try {\n"
         "        if constexpr (std::is_void_v<std::invoke_result_t<Work, "
         "transaction&>>) {\n"
         "            std::forward<Work>(work)(tx);\n"
         "            tx.commit();\n"
         "        } else {\n"
         "            auto out = std::forward<Work>(work)(tx);\n"
         "            tx.commit();\n"
         "            return out;\n"
         "        }\n"
         "    } catch (...) {\n"
         "        tx.rollback();\n"
         "        throw;\n"
         "    }\n"
         "}\n"
         "}\n"},
        {"projects/ores.a/include/ores.a/names.hpp",
         "#include \"ores.database/pq/session_context.hpp\"\n"
         "void f(const ores::database::pq::session_context& c);\n"}});
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.a/include/ores.a/names.hpp", 1}});
}

TEST_CASE("a list file that lists other rows than the types option is "
    "reported on the file, naming the row and the value to set",
    "[codecs][forward_declarable_includes]") {
    const auto r = tests::run(listing(both_types),
        with_list("ores::db::pool  struct  ores.db/pq/pool.hpp  -"));
    REQUIRE(tests::places(r.findings) == places{{std::string(list_path), 1}});
    const auto& message = std::get<2>(r.findings[0]);
    CHECK(message.find("the file has ores::db::pool;struct;"
        "ores.db/pq/pool.hpp;- where the option has ores::db::pool;struct;"
        "ores.db/pool.hpp;-") != std::string::npos);
    CHECK(message.find("ores::db::pq::context") == std::string::npos);
    CHECK(r.fixes[0] == "set types=ores::db::pq::context;class;"
        "ores.db/pq/context.hpp;ores.db/pq/context_fwd.hpp,ores::db::pool;"
        "struct;ores.db/pq/pool.hpp;- on the forward_declarable_includes row "
        "of .compass.org");

    const auto extra = tests::run(listing(both_types), with_list(
        "ores::db::pool  struct  ores.db/pool.hpp  -\n"
        "ores::db::row  class  ores.db/row.hpp  -"));
    REQUIRE(extra.findings.size() == 1);
    CHECK(std::get<2>(extra.findings[0]).find(
        "ores::db::row;class;ores.db/row.hpp;- is only in the file") !=
        std::string::npos);
}

TEST_CASE("a list file that lists the rows of the types option, in any "
    "order and spacing, is not reported",
    "[codecs][forward_declarable_includes]") {
    auto tree = with_list("");
    tree[std::string(list_path)] =
        "\tores::db::pool struct ores.db/pool.hpp   -\r\n"
        "  # context\r\n"
        "ores::db::pq::context class ores.db/pq/context.hpp "
        "ores.db/pq/context_fwd.hpp";
    CHECK(tests::run(listing(both_types), tree).findings.empty());
}

TEST_CASE("no list file, or an empty list_file, compares nothing",
    "[codecs][forward_declarable_includes]") {
    auto tree = with_list("ores::db::pool  struct  ores.db/pq/pool.hpp  -");
    CHECK(tests::run(listing(both_types, ""), tree).findings.empty());
    tree.erase(std::string(list_path));
    CHECK(tests::run(listing(both_types), tree).findings.empty());
}

TEST_CASE("a malformed list file is reported on the file",
    "[codecs][forward_declarable_includes]") {
    const auto r = tests::run(listing(both_types),
        with_list("ores::db::pool  union  ores.db/pool.hpp  -"));
    REQUIRE(tests::places(r.findings) == places{{std::string(list_path), 1}});
    CHECK(std::get<2>(r.findings[0]).find("line 4 gives ores::db::pool the "
        "class key union") != std::string::npos);

    auto empty = with_list("");
    empty[std::string(list_path)] = "# nothing\n";
    const auto e = tests::run(listing(both_types), empty);
    REQUIRE(e.findings.size() == 1);
    CHECK(std::get<2>(e.findings[0]).find("no type is listed") !=
        std::string::npos);
}

TEST_CASE("an edit to the list file calls the reduce again, as it read the "
    "file", "[codecs][forward_declarable_includes]") {
    const auto matching = with_list("ores::db::pool struct ores.db/pool.hpp -");
    const auto stale = with_list("ores::db::pool struct ores.db/pq/pool.hpp -");
    CHECK(tests::places(tests::run_after_edit(listing(both_types), matching,
        stale).findings) == places{{std::string(list_path), 1}});
    CHECK(tests::run_after_edit(listing(both_types), stale,
        matching).findings.empty());
}

TEST_CASE("a defining header the tree has that does not define its type "
    "is reported on that header, and an edit to it calls the reduce again",
    "[codecs][forward_declarable_includes]") {
    const files defined{
        {"projects/ores.db/include/ores.db/pq/context.hpp",
         "#pragma once\n"
         "namespace ores::db {\n"
         "namespace pq {\n"
         "class ORES_DB_EXPORT context final { int x; };\n"
         "}\n"
         "}\n"},
        {"projects/ores.db/include/ores.db/pool.hpp",
         "namespace ores::db { struct pool { int y; }; }\n"}};
    CHECK(tests::run(listing(both_types, ""), defined).findings.empty());

    auto moved = defined;
    moved["projects/ores.db/include/ores.db/pq/context.hpp"] =
        "#pragma once\n"
        "#include \"ores.db/pq/context_fwd.hpp\"\n"
        "namespace ores::db::pq { class context; struct other {}; }\n";
    const auto r = tests::run_after_edit(listing(both_types, ""), defined,
        moved);
    REQUIRE(tests::places(r.findings) == places{
        {"projects/ores.db/include/ores.db/pq/context.hpp", 1}});
    CHECK(std::get<2>(r.findings[0]).find("does not define "
        "ores::db::pq::context") != std::string::npos);
}
