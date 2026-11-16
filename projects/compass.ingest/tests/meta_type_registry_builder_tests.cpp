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
#include "compass.ingest/meta/type_registry_builder.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.ingest/org/parser.hpp"
#include "compass.org/ops/type_lookup.hpp"

namespace {

using namespace compass::ingest::meta;
namespace ot = compass::org::types;
namespace ops = compass::org::ops;

contract_report add(type_registry_builder& b, std::string_view page) {
    ot::tables t;
    compass::ingest::org::parse(page, t);
    return b.add_contract(page, t);
}

std::vector<std::string> texts(const ot::type_registry& r,
    ot::column_view<ot::type_entry_id, ot::name_id> v) {
    std::vector<std::string> out;
    for (const auto n : v.values)
        out.emplace_back(ops::name_text(r, n));
    return out;
}

using strings = std::vector<std::string>;

constexpr std::string_view story_page = R"(#+title: Document type: story
#+type: meta

This page defines the =story= document type.

* Before the contract

- Level :: =s5=
- Folder :: =elsewhere/=

* Contract

- Folder :: =versions/<v>/<sprint>/<story_slug>/story.org=
- Level :: =s2=
- TODO vocabulary :: =BACKLOG STARTED | DONE ABANDONED= (no BLOCKED).
- Required sections (in this order) ::
  - /Blurb/ — opens with =This page documents a story=.
  - =* Goal= — the user-visible outcome.
  - =* Status= — table.
  - A =#+begin_export= block, which is not a section.
  - =* Artefacts :noexport:= — the licence.
)";

constexpr std::string_view task_page = R"(#+title: Document type: task

* Contract

- Path :: =versions/<v>/<sprint>/<story>/task_<slug>.org=
- Level :: =s1=

** Task frontmatter

#+begin_src org
,#+owner: <handle>
,#+todo: DISCOVERED BACKLOG STARTED BLOCKED | DONE ABANDONED
#+end_src

** Task sections

Required, in this order.

- =* Goal= — one paragraph.
- =** Result= — filled when the task closes.
)";

}

TEST_CASE("a contract page yields its level, folder, TODO keywords and "
          "sections", "[meta][type_registry]") {
    type_registry_builder b;
    const auto report = add(b, story_page);
    CHECK(fits(report));
    CHECK(report.types == 1);
    CHECK(report.level == level_source::item);
    CHECK(report.folder == folder_source::folder_item);
    CHECK(report.state == state_source::todo_vocabulary_item);
    CHECK(report.sections_from == sections_source::item);
    CHECK(report.sections == 3);

    const auto r = b.build();
    const auto story = ops::find_type(r, "story");
    REQUIRE_FALSE(story.is_none());
    CHECK(r.types.kinds[story] == ot::type_kind::document);
    CHECK(r.types.levels[story] == ot::type_level::s2);
    CHECK(ops::name_text(r, r.types.folders[story]) ==
        "versions/<v>/<sprint>/<story_slug>/story.org");
    CHECK(texts(r, ops::active_todos(r, story)) ==
        strings{"BACKLOG", "STARTED"});
    CHECK(texts(r, ops::done_todos(r, story)) == strings{"DONE", "ABANDONED"});
    CHECK(texts(r, ops::required_sections(r, story)) ==
        strings{"Goal", "Status", "Artefacts"});
}

TEST_CASE("a task-shaped contract reads a Path item, a #+todo: block and a "
          "sections heading", "[meta][type_registry]") {
    type_registry_builder b;
    const auto report = add(b, task_page);
    CHECK(fits(report));
    CHECK(report.folder == folder_source::path_item);
    CHECK(report.state == state_source::todo_block);
    CHECK(report.sections_from == sections_source::heading);

    const auto r = b.build();
    const auto task = ops::find_type(r, "task");
    REQUIRE_FALSE(task.is_none());
    CHECK(r.types.levels[task] == ot::type_level::s1);
    CHECK(texts(r, ops::active_todos(r, task)) ==
        strings{"DISCOVERED", "BACKLOG", "STARTED", "BLOCKED"});
    CHECK(texts(r, ops::done_todos(r, task)) == strings{"DONE", "ABANDONED"});
    CHECK(texts(r, ops::required_sections(r, task)) ==
        strings{"Goal", "Result"});
}

TEST_CASE("a State item carries a #+todo: line and Not stateful carries "
          "none", "[meta][type_registry]") {
    type_registry_builder b;
    const auto design = add(b, "#+title: Document type: design\n"
                               "* Contract\n"
                               "- Folder :: =doc/design/=\n"
                               "- Level :: =s2=\n"
                               "- State :: =#+todo: DRAFT | LANDED=.\n");
    const auto recipe = add(b, "#+title: Document type: recipe\n"
                               "* Contract\n"
                               "- Folder :: =recipes/=\n"
                               "- Level :: =cross=\n"
                               "- Not stateful — no TODO.\n");
    CHECK(design.state == state_source::state_item);
    CHECK(recipe.state == state_source::not_stateful);

    const auto r = b.build();
    const auto d = ops::find_type(r, "design");
    CHECK(texts(r, ops::active_todos(r, d)) == strings{"DRAFT"});
    CHECK(texts(r, ops::done_todos(r, d)) == strings{"LANDED"});
    CHECK(ops::is_stateful(r, d));
    const auto rc = ops::find_type(r, "recipe");
    CHECK(r.types.levels[rc] == ot::type_level::cross);
    CHECK_FALSE(ops::is_stateful(r, rc));
}

TEST_CASE("a page missing a field is reported and registers the type "
          "without it", "[meta][type_registry]") {
    type_registry_builder b;
    const auto report = add(b, "#+title: Document type: plan\n"
                               "* Contract\n"
                               "- Lives inside a task as =* Plan=.\n"
                               "- Folder :: =doc/plans/=\n");
    CHECK_FALSE(fits(report));
    CHECK(report.contract);
    CHECK(report.level == level_source::missing);
    CHECK(report.state == state_source::missing);
    CHECK(report.sections_from == sections_source::missing);
    CHECK(report.folder == folder_source::folder_item);

    const auto r = b.build();
    const auto plan = ops::find_type(r, "plan");
    REQUIRE_FALSE(plan.is_none());
    CHECK(r.types.levels[plan] == ot::type_level::none);
    CHECK(ops::required_sections(r, plan).size() == 0);
}

TEST_CASE("a level the registry does not know is reported as unknown",
    "[meta][type_registry]") {
    type_registry_builder b;
    const auto report = add(b, "#+title: Document type: system\n"
                               "* Contract\n"
                               "- Level :: =s9= or a specific level.\n");
    CHECK(report.level == level_source::unknown_value);
    CHECK_FALSE(fits(report));
}

TEST_CASE("a page without a Contract heading or a type title registers "
          "nothing it cannot name", "[meta][type_registry]") {
    type_registry_builder b;
    const auto no_contract = add(b, "#+title: Document type: decision\n"
                                    "* Notes\n- Level :: =s2=\n");
    CHECK_FALSE(no_contract.contract);
    CHECK(no_contract.level == level_source::missing);

    const auto no_title = add(b, "#+title: Glossary\n"
                                 "* Contract\n- Level :: =s2=\n");
    CHECK(no_title.types == 0);

    const auto r = b.build();
    CHECK(r.types.size() == 1);
    CHECK_FALSE(ops::find_type(r, "decision").is_none());
}

TEST_CASE("one page can define several types that share its contract",
    "[meta][type_registry]") {
    type_registry_builder b;
    const auto report = add(b,
        "#+title: Document type: retrospective, release_note, audit_report\n"
        "* Contract\n- Level :: =s3=\n");
    CHECK(report.types == 3);

    const auto r = b.build();
    for (const auto name : {"retrospective", "release_note", "audit_report"}) {
        const auto t = ops::find_type(r, name);
        REQUIRE_FALSE(t.is_none());
        CHECK(r.types.levels[t] == ot::type_level::s3);
    }
}

TEST_CASE("model types are kept apart from document types",
    "[meta][type_registry]") {
    type_registry_builder b;
    add(b, story_page);
    for (const auto name : model_types)
        CHECK(b.add_model(name));
    CHECK_FALSE(b.add_model("story"));
    CHECK_FALSE(b.add_model("ores.codegen.entity"));

    const auto r = b.build();
    CHECK(r.types.size() == model_types.size() + 1);
    CHECK(r.types.kinds[ops::find_type(r, "story")] ==
        ot::type_kind::document);
    const auto entity = ops::find_type(r, "ores.codegen.entity");
    REQUIRE_FALSE(entity.is_none());
    CHECK(r.types.kinds[entity] == ot::type_kind::model);
    CHECK(r.types.levels[entity] == ot::type_level::none);
    CHECK_FALSE(ops::is_stateful(r, entity));
    CHECK(ops::find_type(r, "entity").is_none());
}

TEST_CASE("a type defined twice keeps its first contract",
    "[meta][type_registry]") {
    type_registry_builder b;
    CHECK_FALSE(add(b, story_page).duplicate);
    const auto again = add(b, "#+title: Document type: story\n"
                              "* Contract\n- Level :: =s4=\n");
    CHECK(again.duplicate);
    CHECK_FALSE(fits(again));

    const auto r = b.build();
    CHECK(r.types.size() == 1);
    CHECK(r.types.levels[ops::find_type(r, "story")] == ot::type_level::s2);
}
