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
#include <array>
#include <cstddef>
#include <map>
#include <set>
#include <span>
#include <utility>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.cli/app/application.hpp"
#include "compass.cli/app/codecs.hpp"
#include "compass.codec/codecs/catalogue.hpp"
#include "compass.codec/ops/exceptions.hpp"
#include "compass.codegen/codecs/catalogue.hpp"
#include "compass.cpp/codecs/catalogue.hpp"
#include "compass.crosscheck/codecs/catalogue.hpp"
#include "compass.docs/codecs/catalogue.hpp"
#include "compass.sql/codecs/catalogue.hpp"
#include "scoped_git_repo.hpp"

namespace {

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

/**
 * @brief The exit status and standard output of compass run with @p args
 * on the checkout of @p repo.
 */
std::pair<int, std::string> run(const compass::cli::tests::scoped_git_repo&
    repo, std::vector<std::string> args) {
    args.insert(args.begin() + 1, {"-C", repo.checkout().generic_string()});
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in;
    std::ostringstream out;
    std::ostringstream err;
    const auto status = compass::cli::app::run(views, "/", in, out, err);
    return {status, out.str()};
}

using namespace compass;
namespace ct = codec::types;

bool is_uuid(std::string_view s) {
    constexpr std::string_view shape = "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX";
    if (s.size() != shape.size())
        return false;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        const bool hex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
        if (shape[i] == '-' ? c != '-' : !hex)
            return false;
    }
    return true;
}

/**
 * @brief Each defect of @p r, named with @p codec, or nothing.
 */
std::vector<std::string> defects_of(std::string_view codec,
    const ct::rule& r) {
    const auto id = std::string(codec) + "/" + std::string(r.name);
    std::vector<std::string> out;
    if (r.summary.empty() || !r.summary.ends_with('.'))
        out.push_back(id + ": the statement is not one sentence");
    if (r.summary.contains('\n'))
        out.push_back(id + ": the statement spans lines");
    std::set<std::string_view> seen;
    for (const auto source : r.enforces) {
        if (!is_uuid(source))
            out.push_back(id + ": " + std::string(source) +
                " is not an uppercase UUID");
        if (!seen.insert(source).second)
            out.push_back(id + ": cites " + std::string(source) + " twice");
    }
    return out;
}

}

TEST_CASE("every rule compass knows has a one-line statement and cites "
    "memories by UUID", "[app][codecs]") {
    std::vector<std::string> defects;
    for (const auto& d : cli::app::codecs()) {
        std::set<std::string_view> names;
        for (const auto& r : d.rules) {
            if (!names.insert(r.name).second)
                defects.push_back(std::string(d.name) + "/" +
                    std::string(r.name) + ": declared twice");
            for (auto& m : defects_of(d.name, r))
                defects.push_back(std::move(m));
        }
    }
    for (const auto& r : codec::ops::exception_rules)
        for (auto& m : defects_of(codec::ops::exception_codec, r))
            defects.push_back(std::move(m));
    for (auto& m : defects_of("<files codec>", codec::ops::drift_rule))
        defects.push_back(std::move(m));
    CHECK(defects == std::vector<std::string>{});
}

TEST_CASE("the check of a rule's metadata refuses a malformed citation",
    "[app][codecs]") {
    constexpr std::array<std::string_view, 3> sources{
        "5b528519-8bce-4cb8-a48e-b525fd71f873",
        "5B528519-8BCE-4CB8-A48E-B525FD71F87",
        "5B528519-8BCE-4CB8-A48E-B525FD71F873"};
    const ct::rule lowercase{"r", "A statement.", {sources.data(), 1}};
    const ct::rule short_id{"r", "A statement.", {sources.data() + 1, 1}};
    const ct::rule good{"r", "A statement.", {sources.data() + 2, 1}};
    const std::array twice{sources[2], sources[2]};
    const ct::rule repeated{"r", "A statement.", twice};
    const ct::rule unfinished{"r", "A statement", {}};

    CHECK(defects_of("c", lowercase).size() == 1);
    CHECK(defects_of("c", short_id).size() == 1);
    CHECK(defects_of("c", good).empty());
    CHECK(defects_of("c", repeated) ==
        std::vector<std::string>{"c/r: cites " + std::string(sources[2]) +
            " twice"});
    CHECK(defects_of("c", unfinished) ==
        std::vector<std::string>{"c/r: the statement is not one sentence"});
}

TEST_CASE("every codec compass knows carries the code hash of the "
    "component that defines it", "[app][codecs]") {
    const std::array<std::pair<std::span<const ct::descriptor>,
        std::string_view>, 6> components{{
        {codec::codecs::catalogue(), codec::codecs::code_hash()},
        {codegen::codecs::catalogue(), codegen::codecs::code_hash()},
        {cpp::codecs::catalogue(), cpp::codecs::code_hash()},
        {sql::codecs::catalogue(), sql::codecs::code_hash()},
        {crosscheck::codecs::catalogue(), crosscheck::codecs::code_hash()},
        {docs::codecs::catalogue(), docs::codecs::code_hash()}}};
    std::map<std::string_view, std::string_view> expected;
    std::set<std::string_view> hashes;
    for (const auto& [catalogue, hash] : components) {
        CHECK(hash.size() == 64);
        CHECK(hash.find_first_not_of("0123456789abcdef") ==
            std::string_view::npos);
        hashes.insert(hash);
        for (const auto& d : catalogue)
            expected[d.name] = hash;
    }
    expected["rule_table"] = cli::app::code_hash();
    hashes.insert(cli::app::code_hash());

    std::vector<std::string> defects;
    for (const auto& d : cli::app::codecs()) {
        const auto it = expected.find(d.name);
        if (it == expected.end())
            defects.push_back(std::string(d.name) + ": in no catalogue");
        else if (d.code != it->second)
            defects.push_back(std::string(d.name) + ": code is '" +
                std::string(d.code) + "'");
    }
    CHECK(defects == std::vector<std::string>{});
    CHECK(hashes.size() == components.size() + 1);
}

TEST_CASE("the rule_table codec writes the page under the checkout and "
    "check reports it once a memory changes", "[app][codecs]") {
    compass::cli::tests::scoped_git_repo repo;
    repo.write("doc/meta/document_type_memory.org",
        "#+title: Document type: memory\n#+type: meta\n\n* Contract\n\n"
        "- Folder :: =doc/llm/memory/=\n- Level :: =cross=\n");
    repo.write("doc/llm/memory/no_numbers.org",
        ":PROPERTIES:\n:ID: 177EDB1C-6241-45AA-81B1-46C4B9D6329D\n:END:\n"
        "#+title: Do not number org headings\n#+type: memory\n");
    repo.write(".compass.org", "#+output: .\n* Codecs\n"
        "| document_index | no | |\n| required_sections | no | |\n"
        "| rule_table | yes | path=doc/rules.org |\n");

    REQUIRE(run(repo, {"gen"}).first == 0);
    const auto page = compass::cli::tests::scoped_git_repo::read_file(
        repo.checkout() / "doc" / "rules.org");
    CHECK_THAT(page, ContainsSubstring(
        "| =heading_numbers= | numbered_heading | No org heading carries its "
        "own number. | [[id:177EDB1C-6241-45AA-81B1-46C4B9D6329D][Do not "
        "number org headings]] |\n"));
    CHECK_THAT(page, ContainsSubstring(
        "Of the 1 memories of this repository, 0 are cited by no rule."));
    CHECK(run(repo, {"check"}).first == 0);

    repo.write("doc/llm/memory/no_push.org",
        ":PROPERTIES:\n:ID: 40003D46-DAEB-41DF-B4EE-947E223A8B8F\n:END:\n"
        "#+title: Do not push automatically\n#+type: memory\n");
    const auto [status, out] = run(repo, {"check"});
    CHECK(status == 1);
    CHECK_THAT(out, StartsWith("doc/rules.org:"));
    CHECK_THAT(out, ContainsSubstring(": rule_table/drift: the file differs "
        "from what rule_table generates"));
}
