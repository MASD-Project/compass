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
#include "compass.crosscheck/codecs/paste_blocks.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::crosscheck;
using tests::files;

std::vector<std::string> messages(const tests::outcome& r) {
    std::vector<std::string> out;
    for (const auto& [path, line, message] : r.findings)
        out.push_back(path + ":" + std::to_string(line) + ": " + message);
    return out;
}

files tree(std::string block) {
    return {
        {"projects/ores.refdata/modeling/ores.refdata.currency.org",
         "#+title: currency\n\n* C++\n" + block},
        {"projects/ores.workflow/include/ores.workflow/api/step.hpp",
         "namespace ores::workflow::api {\n"
         "class ORES_WORKFLOW_EXPORT step_runner { void run(); };\n"
         "// namespace ores::workflow { class commented_out {}; }\n"
         "const char* s = \"struct quoted {\";\n"
         "}\n"
         "namespace ores { namespace service {\n"
         "enum class state { idle };\n"
         "} }\n"},
        {"projects/ores.workflow/src/hidden.hpp",
         "namespace ores::workflow::api { class not_a_header {}; }\n"}};
}

}

TEST_CASE("a pasted name resolves against its own scope only",
    "[codecs][paste_blocks]") {
    const auto t = tree(
        "#+begin_src cpp :implements handler :name override\n"
        "ores::workflow::api::step_runner r;\n"
        "ores::service::state s = ores::service::state::idle;\n"
        "ores::workflow::step_runner moved;\n"
        "ores::workflow::api::commented_out c;\n"
        "ores::workflow::api::not_a_header h;\n"
        "ores::workflow::api::run();\n"
        "#+end_src\n");
    CHECK(messages(tests::run(codecs::paste_blocks, t)) ==
        std::vector<std::string>{
            "projects/ores.refdata/modeling/ores.refdata.currency.org:4: "
            "block override: ores::workflow::api::commented_out names "
            "nothing: no namespace or type ores::workflow::api declares "
            "commented_out",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:4: "
            "block override: ores::workflow::api::not_a_header names "
            "nothing: no namespace or type ores::workflow::api declares "
            "not_a_header",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:4: "
            "block override: ores::workflow::api::run names nothing: no "
            "namespace or type ores::workflow::api declares run",
            "projects/ores.refdata/modeling/ores.refdata.currency.org:4: "
            "block override: ores::workflow::step_runner names nothing: no "
            "namespace or type ores::workflow declares step_runner"});
}

TEST_CASE("only a cpp block whose header says :implements is read",
    "[codecs][paste_blocks]") {
    auto t = tree(
        "#+begin_src cpp :name plain\nores::gone::x a;\n#+end_src\n"
        "#+begin_src python :implements x\nores::gone::y b\n#+end_src\n"
        "#+begin_src cpp :implements handler\n"
        "ores::workflow::api::step_runner r;\n#+end_src\n"
        "  #+begin_src cpp :implements indented\nores::gone::z c;\n"
        "  #+end_src\n#+begin_src cpp :implements last\n"
        "ores::gone::w d;\n#+end_src\n");
    const auto r = tests::run(codecs::paste_blocks, t);
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<2>(r.findings[0]) == "block (unnamed): ores::gone::w names "
        "nothing: no namespace or type ores::gone declares w");
}

TEST_CASE("a tree with no block to read is a finding, not a pass",
    "[codecs][paste_blocks]") {
    auto t = tree("#+begin_src cpp :name plain\nint x;\n#+end_src\n");
    const auto r = tests::run(codecs::paste_blocks, t);
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<0>(r.findings[0]) == "projects");

    t = tree("#+begin_src cpp :implements x\nint y;\n#+end_src\n");
    CHECK(std::get<2>(tests::run(codecs::paste_blocks, t).findings.at(0))
        .starts_with("no qualified name found"));
}

TEST_CASE("an edit that renames a declared type reports the block that "
    "names it, and renaming it back clears it", "[codecs][paste_blocks]") {
    const auto before = tree(
        "#+begin_src cpp :implements handler :name use\n"
        "ores::workflow::api::step_runner r;\n"
        "#+end_src\n");
    auto after = before;
    after["projects/ores.workflow/include/ores.workflow/api/step.hpp"] =
        "namespace ores::workflow::api { class runner {}; }\n";

    CHECK(messages(tests::run_after_edit(codecs::paste_blocks, before,
        after)) == std::vector<std::string>{
            "projects/ores.refdata/modeling/ores.refdata.currency.org:4: "
            "block use: ores::workflow::api::step_runner names nothing: no "
            "namespace or type ores::workflow::api declares step_runner"});
    CHECK(tests::run_after_edit(codecs::paste_blocks, after, before)
        .findings.empty());
}
