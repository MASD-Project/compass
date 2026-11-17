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
#include <algorithm>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.cli/app/application.hpp"
#include "compass.cli/io/hook_payload.hpp"
#include "compass.cli/io/listing.hpp"
#include "compass.store/io/object_file.hpp"
#include "scoped_git_repo.hpp"

namespace {

using namespace compass::cli;
using tests::scoped_git_repo;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;
using lines = std::vector<std::string>;

constexpr std::string_view configuration =
    "#+output: .\n\n* Codecs\n\n"
    "| name | enabled | options |\n"
    "| document_index | yes | path=index.tsv |\n"
    "| required_sections | yes | |\n"
    "| sql_hygiene | yes | roots=sql |\n"
    "| sql_reachability | yes | |\n";

constexpr std::string_view today = "2026-11-16";

struct outcome {
    int status = -1;
    std::string out;
    std::string err;
};

outcome run(std::vector<std::string> args, std::string_view input = {}) {
    const std::vector<std::string_view> views(args.begin(), args.end());
    std::istringstream in{std::string(input)};
    std::ostringstream out;
    std::ostringstream err;
    const auto status = app::run(views, "/", in, out, err);
    return {status, out.str(), err.str()};
}

/**
 * @brief The findings of compass's output, as path:line:rule, sorted:
 * the lines of check, or of the reason of the hook's JSON.
 */
lines findings_of(std::string text) {
    if (const auto at = text.find("\"reason\": \""); at != std::string::npos) {
        text = text.substr(at + 11);
        text = std::regex_replace(text, std::regex(R"(\\n)"), "\n");
        text = std::regex_replace(text, std::regex(R"(\\")"), "\"");
    }
    static const std::regex finding("^([^: ]+:[0-9]+):[0-9]+: ([^: ]+): .*");
    lines out;
    std::istringstream in(text);
    std::smatch m;
    for (std::string line; std::getline(in, line);) {
        if (std::regex_match(line, m, finding))
            out.push_back(m[1].str() + ":" + m[2].str());
    }
    std::ranges::sort(out);
    return out;
}

outcome check(const scoped_git_repo& repo) {
    return run({"check", "-C", repo.checkout().string(), "-j", "2",
        "--today", std::string(today)});
}

/**
 * @brief The PostToolUse payload Claude Code sends after @p tool changed
 * @p file, an absolute path or one relative to @p cwd.
 */
std::string payload(const std::string& file, const std::string& cwd,
    std::string_view tool = "Edit") {
    return "{\"session_id\": \"s\", \"transcript_path\": \"/t.jsonl\", "
           "\"cwd\": \"" + cwd + "\", \"permission_mode\": \"default\", "
           "\"hook_event_name\": \"PostToolUse\", \"tool_name\": \"" +
        std::string(tool) + "\", \"tool_input\": {\"file_path\": \"" + file +
        "\", \"old_string\": \"a\", \"new_string\": \"b\"}, "
        "\"tool_response\": {\"filePath\": \"" + file + "\", \"type\": "
        "\"update\"}, \"tool_use_id\": \"toolu_1\"}";
}

outcome hook(const scoped_git_repo& repo, const std::string& file) {
    return run({"hook", "-j", "2", "--today", std::string(today)},
        payload((repo.checkout() / file).string(), "/"));
}

/**
 * @brief The findings of a check on @p file, and those of the check
 * before the edit lacked on other files.
 */
lines expected(const lines& before, const lines& after,
    const std::string& file) {
    lines out;
    auto old = before;
    for (const auto& f : after) {
        if (f.starts_with(file + ":")) {
            out.push_back(f);
            continue;
        }
        const auto it = std::ranges::find(old, f);
        if (it == old.end())
            out.push_back(f);
        else
            old.erase(it);
    }
    return out;
}

void setup(const scoped_git_repo& repo) {
    repo.write(".compass.org", configuration);
    repo.write("sql/a.sql", "select 1;\n");
    repo.write("notes.txt", "Notes.\n");
    repo.write("projects/ores.sql/setup.sql", "\\ir create/create.sql\n");
    repo.write("projects/ores.sql/create/create.sql",
        "\\ir tables/a.sql\n\\ir tables/b.sql\n");
    repo.write("projects/ores.sql/create/tables/a.sql",
        "create table if not exists a (x int);\n");
    repo.write("projects/ores.sql/create/tables/b.sql",
        "create table if not exists b (x int);\n");
    repo.write("projects/ores.sql/create/tables/orphan.sql",
        "create table if not exists c (x int);\n");
    repo.git("add -A");
}

}

TEST_CASE("the hook reports what check reports on the edited file",
    "[app][hook]") {
    scoped_git_repo repo;
    setup(repo);
    const auto before = findings_of(check(repo).out);
    REQUIRE_FALSE(before.empty());
    repo.write("sql/a.sql", "select 1;\ncreate table t (x int);\n");

    const auto r = hook(repo, "sql/a.sql");
    CHECK(r.status == 0);
    CHECK_THAT(r.out, StartsWith("{\"decision\": \"block\", \"reason\": "
        "\"compass found 1 finding on or caused by the edit to sql/a.sql"));
    CHECK(findings_of(r.out) == lines{"sql/a.sql:2:sql_hygiene/"
        "not_re_runnable"});
    CHECK(findings_of(r.out) ==
        expected(before, findings_of(check(repo).out), "sql/a.sql"));
}

TEST_CASE("an edit that leaves another file unreachable reports that file "
    "and not what other files already had", "[app][hook]") {
    scoped_git_repo repo;
    setup(repo);
    const auto before = findings_of(check(repo).out);
    REQUIRE(std::ranges::contains(before,
        "projects/ores.sql/create/tables/orphan.sql:1:sql_reachability/"
        "unreachable_file"));
    repo.write("projects/ores.sql/create/create.sql", "\\ir tables/a.sql\n");

    const auto r = hook(repo, "projects/ores.sql/create/create.sql");
    const lines wanted{"projects/ores.sql/create/tables/b.sql:1:"
        "sql_reachability/unreachable_file"};
    CHECK(findings_of(r.out) == wanted);
    CHECK(expected(before, findings_of(check(repo).out),
        "projects/ores.sql/create/create.sql") == wanted);
}

TEST_CASE("an edit to a document reports the drift of the output it feeds",
    "[app][hook]") {
    scoped_git_repo repo;
    setup(repo);
    REQUIRE(run({"generate", "-C", repo.checkout().string(), "-j", "2"})
        .status == 0);
    const auto before = findings_of(check(repo).out);
    repo.write("doc/task_a.org", ":PROPERTIES:\n:ID: A-1\n:END:\n"
        "#+title: Task: renamed\n#+type: task\n* Goal\n* Result\n");

    const auto r = hook(repo, "doc/task_a.org");
    CHECK(findings_of(r.out) == lines{"index.tsv:3:document_index/drift"});
    CHECK(findings_of(r.out) ==
        expected(before, findings_of(check(repo).out), "doc/task_a.org"));
}

TEST_CASE("an edit to a generated file reports its drift", "[app][hook]") {
    scoped_git_repo repo;
    setup(repo);
    REQUIRE(run({"generate", "-C", repo.checkout().string(), "-j", "2"})
        .status == 0);
    REQUIRE(check(repo).status == 1);
    repo.write("index.tsv", "edited by hand\n");

    const auto r = hook(repo, "index.tsv");
    CHECK(findings_of(r.out) == lines{"index.tsv:1:document_index/drift"});
    CHECK_THAT(r.out, ContainsSubstring("edited after compass wrote it"));
}

TEST_CASE("a new document is checked by every document codec",
    "[app][hook]") {
    scoped_git_repo repo;
    setup(repo);
    REQUIRE(run({"generate", "-C", repo.checkout().string(), "-j", "2"})
        .status == 0);
    const auto before = findings_of(check(repo).out);
    repo.write("doc/task_c.org",
        "#+title: Task: c\n#+type: task\n* Goal\n");
    auto write = payload((repo.checkout() / "doc/task_c.org").string(), "/",
        "Write");
    write.replace(write.find("\"update\""), 8, "\"create\"");

    const auto r = run({"hook", "-j", "2", "--today", std::string(today)},
        write);
    CHECK(findings_of(r.out) == lines{
        "doc/task_c.org:2:required_sections/missing_section",
        "index.tsv:5:document_index/drift"});
    CHECK(findings_of(r.out) ==
        expected(before, findings_of(check(repo).out), "doc/task_c.org"));
}

TEST_CASE("a generator that refuses after a good run is reported and keeps "
    "its file, in the hook, in check and in generate", "[app][hook]") {
    scoped_git_repo repo;
    repo.write(".compass.org", "#+output: .\n\n* Codecs\n\n"
        "| name | enabled | options |\n"
        "| document_index | no | |\n"
        "| required_sections | no | |\n"
        "| ore_renderings_tables | yes | |\n");
    const std::string json = "projects/ores.sql/utility/ore_renderings.json";
    const std::string header =
        "projects/ores.ore/core/src/domain/conventions_renderings.hpp";
    repo.write(json, "{\"tables\": {\"a\": {\"A\": [\"A\"]}}}");
    repo.git("add -A");
    REQUIRE(run({"generate", "-C", repo.checkout().string(), "-j", "2"})
        .status == 0);
    const auto good = scoped_git_repo::read_file(repo.checkout() / header);
    REQUIRE_FALSE(good.empty());
    REQUIRE(check(repo).status == 0);

    repo.write(json, "{\"tables\": ");
    const lines refused{header + ":1:ore_renderings_tables/refused"};
    CHECK(findings_of(hook(repo, json).out) == refused);
    CHECK(findings_of(check(repo).out) == refused);
    CHECK(run({"generate", "-C", repo.checkout().string(), "-j", "2"})
        .status == 0);
    CHECK(scoped_git_repo::read_file(repo.checkout() / header) == good);
}

TEST_CASE("inline exceptions apply to the hook's findings", "[app][hook]") {
    scoped_git_repo repo;
    setup(repo);
    REQUIRE(check(repo).status == 1);
    repo.write("sql/a.sql", "create table t (x int); -- compass:except("
        "sql_hygiene/not_re_runnable) expires=2026-12-31 approver=marco "
        "reason=bootstrap\n");
    const auto excused = hook(repo, "sql/a.sql");
    CHECK(excused.status == 0);
    CHECK(excused.out.empty());

    repo.write("sql/a.sql", "-- compass:except(sql_hygiene/not_re_runnable) "
        "expires=2026-11-15 approver=marco reason=bootstrap\n"
        "create table t (x int);\n");
    CHECK(findings_of(hook(repo, "sql/a.sql").out) == lines{
        "sql/a.sql:2:exception/expired",
        "sql/a.sql:2:sql_hygiene/not_re_runnable"});
}

TEST_CASE("a file no codec reads, or outside any checkout, prints nothing",
    "[app][hook]") {
    scoped_git_repo repo;
    setup(repo);
    REQUIRE(check(repo).status == 1);
    repo.write("notes.txt", "Other notes.\n");

    const auto unread = hook(repo, "notes.txt");
    CHECK(unread.status == 0);
    CHECK(unread.out.empty());

    const auto outside = run({"hook"},
        payload("/nonexistent-compass-dir/notes.txt", "/"));
    CHECK(outside.status == 0);
    CHECK(outside.out.empty());
}

TEST_CASE("a relative file path is taken against the payload's cwd",
    "[app][hook]") {
    scoped_git_repo repo;
    setup(repo);
    REQUIRE(check(repo).status == 1);
    repo.write("sql/a.sql", "create table t (x int);\n");

    const auto r = run({"hook", "--today", std::string(today)},
        payload("sql/a.sql", repo.checkout().string()));
    CHECK(findings_of(r.out) == lines{"sql/a.sql:1:sql_hygiene/"
        "not_re_runnable"});
}

TEST_CASE("the hook fails open with a note to the agent", "[app][hook]") {
    scoped_git_repo repo;
    setup(repo);

    const auto unchecked = hook(repo, "sql/a.sql");
    CHECK(unchecked.status == 0);
    CHECK_THAT(unchecked.out, StartsWith("{\"hookSpecificOutput\": "
        "{\"hookEventName\": \"PostToolUse\", \"additionalContext\": "
        "\"compass hook: no dependency record yet; run compass check once"));

    const auto garbage = run({"hook"}, "not json");
    CHECK(garbage.status == 0);
    CHECK_THAT(garbage.out, ContainsSubstring("the hook payload is not JSON"));

    REQUIRE(check(repo).status == 1);
    repo.write(".git/compass/deps", "damaged");
    const auto damaged = hook(repo, "sql/a.sql");
    CHECK(damaged.status == 0);
    CHECK_THAT(damaged.out, ContainsSubstring("dependency record cannot be "
        "used"));

    const auto contract = hook(repo, "doc/meta/document_type_task.org");
    CHECK_THAT(contract.out, ContainsSubstring("changes what every codec "
        "sees; run compass check"));
}

TEST_CASE("a payload names its file, its tool and whether it may be new",
    "[io][hook]") {
    const auto edit = io::parse_hook_payload(payload("/r/a.sql", "/"));
    REQUIRE(edit);
    CHECK(edit->file == "/r/a.sql");
    CHECK(edit->tool == "Edit");
    CHECK_FALSE(edit->created);

    const auto write = io::parse_hook_payload(
        "{\"cwd\": \"/r\", \"tool_name\": \"Write\", \"tool_input\": "
        "{\"file_path\": \"x/../b.sql\", \"content\": \"\"}, "
        "\"tool_response\": {\"type\": \"create\"}}");
    REQUIRE(write);
    CHECK(write->file == "/r/b.sql");
    CHECK(write->created);

    CHECK_FALSE(io::parse_hook_payload("{\"tool_input\": {}}"));
    CHECK_FALSE(io::parse_hook_payload("[1, 2]"));
}

TEST_CASE("an edit to a document created since the last check is checked, "
    "though the check's listing lacks it", "[app][hook]") {
    scoped_git_repo repo;
    setup(repo);
    (void)check(repo);
    repo.write("doc/task_c.org",
        "#+title: Task: c\n#+type: task\n* Goal\n");

    const auto r = hook(repo, "doc/task_c.org");
    CHECK(findings_of(r.out) == lines{
        "doc/task_c.org:2:required_sections/missing_section",
        "index.tsv:1:document_index/drift"});
}

TEST_CASE("the listing record reads back what was written, and a damaged "
    "one reads as nothing", "[app][hook]") {
    scoped_git_repo repo;
    const auto path = repo.checkout() / "listing";
    const std::vector<std::string> files{"a.org", "b.txt", "c/d.org"};
    const std::vector<std::string_view> documents{files[0], files[2]};
    CHECK(io::write_listing_file(path, files, documents));
    CHECK_FALSE(io::write_listing_file(path, files, documents));

    const auto r = io::read_listing_file(path);
    REQUIRE(r);
    CHECK(r->files == files);
    CHECK(r->documents == std::vector<std::uint8_t>{1, 0, 1});

    auto bytes = *compass::store::io::read_file(path);
    bytes[10] = 'x';
    repo.write("listing", bytes);
    CHECK_FALSE(io::read_listing_file(path));
}

namespace {

std::string component_model(std::string_view name) {
    return "#+title: ores." + std::string(name) +
        "\n#+type: ores.codegen.component\n#+name: " + std::string(name) +
        "\n#+full_name: ores." + std::string(name) +
        "\n#+brief: B\n#+component_kind: flat\n";
}

/**
 * @brief A checkout whose code generation writes, for each of the
 * component models a and b, a header of its own, and one summary both
 * name, which is b's since b is the later.
 */
void setup_codegen(const scoped_git_repo& repo) {
    repo.write(".compass.org", "#+output: .\n\n* Codecs\n\n"
        "| name | enabled | options |\n"
        "| codegen | yes | library=lib convention_component=a year=2031 |\n");
    repo.write("lib/data/licence-GPL-v3.txt", "Licence.\n");
    repo.write("lib/data/modeline.org", "#+title: Modelines\n");
    repo.write("lib/component_catalogue.org",
        "* Components\n\n| name | modeling_dir | exclude_org_types |\n"
        "|------+--------------+-------------------|\n"
        "| a | projects/ores.a/modeling | |\n"
        "| b | projects/ores.b/modeling | |\n");
    repo.write("lib/templates/ores.cpp.component.org",
        "#+title: ores.cpp.component\n#+type: facet\n"
        "#+facet_group: ores.cpp\n#+model_types: component\n");
    repo.write("lib/templates/ores.cpp.component.header.org",
        "#+title: ores.cpp.component.header\n#+type: archetype\n"
        "#+facet: ores.cpp.component\n#+component_kind: flat\n"
        "#+output: projects/{component_dir}/own.txt\n"
        "#+begin_src mustache :tangle own.mustache\n#+end_src\n");
    repo.write("lib/templates/ores.cpp.component.summary.org",
        "#+title: ores.cpp.component.summary\n#+type: archetype\n"
        "#+facet: ores.cpp.component\n#+component_kind: flat\n"
        "#+output: projects/summary.txt\n"
        "#+begin_src mustache :tangle summary.mustache\n#+end_src\n");
    repo.write("lib/templates/own.mustache", "own {{component.brief}}\n");
    repo.write("lib/templates/summary.mustache",
        "last {{component.brief}}\n");
    repo.write("projects/ores.a/modeling/component_overview.org",
        component_model("a"));
    repo.write("projects/ores.b/modeling/component_overview.org",
        component_model("b"));
    repo.git("add -A");
    REQUIRE(run({"generate", "-C", repo.checkout().string(), "-j", "2"})
        .status == 0);
}

/**
 * @brief Writes @p text to @p file and checks the hook's findings are
 * @p wanted and are those a full check gains.
 */
void edit_and_compare(const scoped_git_repo& repo, const std::string& file,
    const std::string& text, const lines& wanted) {
    const auto before = findings_of(check(repo).out);
    repo.write(file, text);
    const auto r = hook(repo, file);
    CHECK(findings_of(r.out) == wanted);
    CHECK(findings_of(r.out) ==
        expected(before, findings_of(check(repo).out), file));
}

}

TEST_CASE("an edit to a model reports the drift of that model's outputs "
    "alone", "[app][hook][codegen]") {
    scoped_git_repo repo;
    setup_codegen(repo);
    auto model = component_model("b");
    model.replace(model.find("brief: B"), 8, "brief: C");

    edit_and_compare(repo, "projects/ores.b/modeling/component_overview.org",
        model, {"projects/ores.b/own.txt:1:codegen/drift",
            "projects/summary.txt:1:codegen/drift"});
}

TEST_CASE("an output two models name drifts only when the later model's "
    "changes", "[app][hook][codegen]") {
    scoped_git_repo repo;
    setup_codegen(repo);
    auto model = component_model("a");
    model.replace(model.find("brief: B"), 8, "brief: C");

    edit_and_compare(repo, "projects/ores.a/modeling/component_overview.org",
        model, {"projects/ores.a/own.txt:1:codegen/drift"});
}

TEST_CASE("an edit to a template reports the drift of that template's "
    "outputs", "[app][hook][codegen]") {
    scoped_git_repo repo;
    setup_codegen(repo);

    edit_and_compare(repo, "lib/templates/own.mustache",
        "mine {{component.brief}}\n",
        {"projects/ores.a/own.txt:1:codegen/drift",
            "projects/ores.b/own.txt:1:codegen/drift"});
}

TEST_CASE("a model the catalogue gains is generated by the hook",
    "[app][hook][codegen]") {
    scoped_git_repo repo;
    setup_codegen(repo);
    repo.write("projects/ores.c/modeling/component_overview.org",
        component_model("c"));
    repo.git("add -A");
    (void)check(repo);

    edit_and_compare(repo, "lib/component_catalogue.org",
        "* Components\n\n| name | modeling_dir | exclude_org_types |\n"
        "|------+--------------+-------------------|\n"
        "| a | projects/ores.a/modeling | |\n"
        "| b | projects/ores.b/modeling | |\n"
        "| c | projects/ores.c/modeling | |\n",
        {"projects/ores.c/own.txt:1:codegen/drift"});
}
