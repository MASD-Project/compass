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
#include "compass.org/types/tables.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace compass::org::types;

constexpr std::string_view source =
    ":PROPERTIES:\n"
    ":ID: 80278EB3-59DC-4549-A790-14E20197F2C6\n"
    ":END:\n"
    "#+title: Define the org tables\n"
    "#+TYPE: task\n"
    "\n"
    "* Goal\n"
    "\n"
    "Name the *tables*; see [[id:F1C69FBB][the story]].\n"
    "\n"
    "* STARTED Status :work:\n"
    "\n"
    "| Field | Value     |\n"
    "|-------+-----------|\n"
    "| State | =STARTED= |\n"
    "\n"
    "#+begin_src sh\n"
    "./compass.sh lint\n"
    "#+end_src\n";

/**
 * Fills the tables by hand, in the order a parser would: an element is
 * opened, its own objects and its children are added, and it is closed.
 */
class builder {
public:
    source_range at(std::string_view needle) const {
        const auto begin = source.find(needle);
        REQUIRE(begin != std::string_view::npos);
        return {static_cast<std::uint32_t>(begin),
            static_cast<std::uint32_t>(begin + needle.size())};
    }

    source_range from(std::string_view first, std::string_view last) const {
        return {at(first).begin, at(last).end};
    }

    name_id intern(std::string_view name) {
        for (std::uint32_t n = 0; n < t.names.size(); ++n) {
            if (name_text(name_id{n}) == name)
                return {n};
        }
        t.names.firsts.values.push_back(
            static_cast<std::uint32_t>(t.name_bytes.size()));
        t.name_bytes.insert(t.name_bytes.end(), name.begin(), name.end());
        return {static_cast<std::uint32_t>(t.names.size() - 1)};
    }

    element_id open(element_kind kind, source_range range,
        source_range contents = {}) {
        const element_id e{static_cast<std::uint32_t>(t.elements.size())};
        t.elements.kinds.values.push_back(kind);
        t.elements.post_blanks.values.push_back(0);
        t.elements.ranges.values.push_back(range);
        t.elements.contents.values.push_back(contents);
        t.elements.parents.values.push_back(
            open_.empty() ? element_id::none() : open_.back());
        t.elements.subtree_ends.values.push_back(element_id::none());
        t.elements.object_firsts.values.push_back(next_object());
        open_.push_back(e);
        return e;
    }

    void close() {
        t.elements.subtree_ends[open_.back()] =
            {static_cast<std::uint32_t>(t.elements.size())};
        open_.pop_back();
    }

    element_id leaf(element_kind kind, source_range range,
        source_range contents = {}) {
        const auto e = open(kind, range, contents);
        close();
        return e;
    }

    object_id object(object_kind kind, source_range range,
        source_range contents = {}, std::uint32_t nested = 0) {
        const auto o = next_object();
        t.objects.kinds.values.push_back(kind);
        t.objects.post_blanks.values.push_back(0);
        t.objects.ranges.values.push_back(range);
        t.objects.contents.values.push_back(contents);
        t.objects.subtree_ends.values.push_back({o.value + 1 + nested});
        return o;
    }

    std::string_view name_text(name_id n) const {
        const auto first = t.names.firsts[n];
        const auto end = n.next().value < t.names.size()
            ? t.names.firsts[n.next()]
            : static_cast<std::uint32_t>(t.name_bytes.size());
        return std::string_view(t.name_bytes.data(), t.name_bytes.size())
            .substr(first, end - first);
    }

    tables t;

private:
    object_id next_object() const {
        return {static_cast<std::uint32_t>(t.objects.size())};
    }

    std::vector<element_id> open_;
};

void add_keyword(builder& b, std::string_view line, std::string_view name,
    std::string_view value) {
    const auto e = b.leaf(element_kind::keyword, b.at(line));
    b.t.keywords.elements.values.push_back(e);
    b.t.keywords.names.values.push_back(b.intern(name));
    b.t.keywords.values.values.push_back(b.at(value));
}

void add_heading(builder& b, element_id e, std::string_view title,
    std::string_view todo, todo_type type) {
    auto& h = b.t.headings;
    h.elements.values.push_back(e);
    h.levels.values.push_back(1);
    h.todos.values.push_back(todo.empty() ? name_id::none() : b.intern(todo));
    h.todo_types.values.push_back(type);
    h.priorities.values.push_back(0);
    h.comments.values.push_back(comment_state::live);
    h.titles.values.push_back(b.at(title));
    h.tag_firsts.values.push_back(
        {static_cast<std::uint32_t>(b.t.heading_tags.size())});
    h.property_firsts.values.push_back(
        {static_cast<std::uint32_t>(b.t.properties.size())});
}

void add_cell(builder& b, std::string_view cell, std::string_view text,
    std::uint32_t nested = 0) {
    b.object(object_kind::table_cell, b.at(cell), b.at(text), nested);
}

builder example() {
    builder b;
    const source_range all{0, static_cast<std::uint32_t>(source.size())};
    b.open(element_kind::document, all, all);

    b.open(element_kind::section, b.from(":PROPERTIES:", "task\n\n"));
    b.open(element_kind::property_drawer, b.from(":PROPERTIES:", ":END:\n"));
    const auto id = b.leaf(element_kind::node_property, b.from(":ID:", "C6\n"));
    b.t.properties.elements.values.push_back(id);
    b.t.properties.names.values.push_back(b.intern("ID"));
    b.t.properties.values.values.push_back(
        b.at("80278EB3-59DC-4549-A790-14E20197F2C6"));
    b.close();
    add_keyword(b, "#+title: Define the org tables\n", "title",
        "Define the org tables");
    add_keyword(b, "#+TYPE: task\n", "type", "task");
    b.close();

    const auto goal = b.open(element_kind::heading,
        b.from("* Goal", "story]].\n\n"), b.from("Name the", "story]].\n\n"));
    add_heading(b, goal, "Goal", "", todo_type::none);
    b.open(element_kind::section, b.from("Name the", "story]].\n\n"));
    const auto text = b.from("Name the", "story]].");
    b.open(element_kind::paragraph, b.from("Name the", "story]].\n\n"), text);
    b.object(object_kind::bold, b.at("*tables*"), b.at("tables"));
    const auto link = b.object(object_kind::link,
        b.at("[[id:F1C69FBB][the story]]"), b.at("the story"));
    b.t.links.objects.values.push_back(link);
    b.t.links.types.values.push_back(b.intern("id"));
    b.t.links.formats.values.push_back(link_format::bracket);
    b.t.links.paths.values.push_back(b.at("F1C69FBB"));
    b.close();
    b.close();
    b.close();

    const auto status = b.open(element_kind::heading,
        b.from("* STARTED", "#+end_src\n"), b.from("| Field", "#+end_src\n"));
    add_heading(b, status, "Status", "STARTED", todo_type::todo);
    b.t.heading_tags.names.values.push_back(b.intern("work"));
    b.open(element_kind::section, b.from("| Field", "#+end_src\n"));
    b.open(element_kind::table, b.from("| Field", "=STARTED= |\n\n"),
        b.from("| Field", "=STARTED= |\n"));
    b.open(element_kind::table_row, b.at("| Field | Value     |\n"),
        b.at(" Field | Value     |"));
    add_cell(b, " Field |", "Field");
    add_cell(b, " Value     |", "Value");
    b.close();
    b.leaf(element_kind::table_rule, b.at("|-------+-----------|\n"));
    b.open(element_kind::table_row, b.at("| State | =STARTED= |\n"),
        b.at(" State | =STARTED= |"));
    add_cell(b, " State |", "State");
    add_cell(b, " =STARTED= |", "=STARTED=", 1);
    b.object(object_kind::verbatim, b.at("=STARTED="));
    b.close();
    b.close();
    const auto src =
        b.leaf(element_kind::src_block, b.from("#+begin_src", "#+end_src\n"));
    b.t.blocks.elements.values.push_back(src);
    b.t.blocks.names.values.push_back(b.intern("sh"));
    b.t.blocks.parameters.values.push_back({});
    b.t.blocks.bodies.values.push_back(b.at("./compass.sh lint\n"));
    b.close();
    b.close();

    b.close();
    return b;
}

std::vector<element_id> children(const tables& t, element_id e) {
    std::vector<element_id> r;
    for (auto c = e.next(); c < t.elements.subtree_ends[e];
         c = t.elements.subtree_ends[c])
        r.push_back(c);
    return r;
}

std::optional<element_id> child_of_kind(const tables& t, element_id e,
    element_kind kind) {
    for (const auto c : children(t, e)) {
        if (t.elements.kinds[c] == kind)
            return c;
    }
    return std::nullopt;
}

object_id objects_end(const tables& t, element_id e) {
    return e.next().value < t.elements.size()
        ? t.elements.object_firsts[e.next()]
        : object_id{static_cast<std::uint32_t>(t.objects.size())};
}

std::vector<object_id> own_objects(const tables& t, element_id e) {
    std::vector<object_id> r;
    for (auto o = t.elements.object_firsts[e]; o < objects_end(t, e);
         o = t.objects.subtree_ends[o])
        r.push_back(o);
    return r;
}

element_id container(const tables& t, object_id o) {
    const auto& firsts = t.elements.object_firsts.values;
    const auto after = std::upper_bound(firsts.begin(), firsts.end(), o);
    return {static_cast<std::uint32_t>(after - firsts.begin() - 1)};
}

std::optional<heading_id> heading_of(const tables& t, element_id e) {
    const auto& elements = t.headings.elements.values;
    const auto at = std::lower_bound(elements.begin(), elements.end(), e);
    if (at == elements.end() || *at != e)
        return std::nullopt;
    return heading_id{static_cast<std::uint32_t>(at - elements.begin())};
}

std::optional<heading_id> enclosing_heading(const tables& t, element_id e) {
    for (; !e.is_none(); e = t.elements.parents[e]) {
        if (t.elements.kinds[e] == element_kind::heading)
            return heading_of(t, e);
    }
    return std::nullopt;
}

std::optional<heading_id> heading_titled(const tables& t,
    std::string_view title) {
    for (std::uint32_t h = 0; h < t.headings.size(); ++h) {
        if (t.headings.titles[heading_id{h}].of(source) == title)
            return heading_id{h};
    }
    return std::nullopt;
}

}

TEST_CASE("a keyword is found by its lower-case name", "[types][tables]") {
    const auto b = example();
    const auto& k = b.t.keywords;

    std::vector<std::string_view> values;
    for (std::uint32_t i = 0; i < k.size(); ++i) {
        if (b.name_text(k.names[keyword_id{i}]) == "type")
            values.push_back(k.values[keyword_id{i}].of(source));
    }

    REQUIRE(values.size() == 1);
    CHECK(values.front() == "task");
}

TEST_CASE("the properties before the first heading belong to the document",
    "[types][tables]") {
    const auto b = example();
    const auto& t = b.t;

    const auto end = t.headings.property_firsts[heading_id{0}];
    REQUIRE(end == property_id{1});
    CHECK(b.name_text(t.properties.names[property_id{0}]) == "ID");
    CHECK(t.properties.values[property_id{0}].of(source) ==
        "80278EB3-59DC-4549-A790-14E20197F2C6");
    CHECK(t.elements.kinds[t.properties.elements[property_id{0}]] ==
        element_kind::node_property);
}

TEST_CASE("the Status table of a task yields its state", "[types][tables]") {
    const auto b = example();
    const auto& t = b.t;

    const auto status = heading_titled(t, "Status");
    REQUIRE(status);
    const auto section = child_of_kind(t, t.headings.elements[*status],
        element_kind::section);
    REQUIRE(section);
    const auto table = child_of_kind(t, *section, element_kind::table);
    REQUIRE(table);

    std::string_view state;
    std::size_t rules = 0;
    for (const auto row : children(t, *table)) {
        if (t.elements.kinds[row] == element_kind::table_rule) {
            ++rules;
            continue;
        }
        const auto cells = own_objects(t, row);
        REQUIRE(cells.size() == 2);
        if (t.objects.contents[cells[0]].of(source) == "State")
            state = t.objects.contents[cells[1]].of(source);
    }

    CHECK(rules == 1);
    CHECK(state == "=STARTED=");
}

TEST_CASE("an object nested in a cell is skipped by the walk over cells",
    "[types][tables]") {
    const auto b = example();
    const auto& t = b.t;

    std::vector<object_kind> kinds;
    for (std::uint32_t o = 0; o < t.objects.size(); ++o)
        kinds.push_back(t.objects.kinds[object_id{o}]);
    REQUIRE(std::ranges::count(kinds, object_kind::verbatim) == 1);

    const auto verbatim = object_id{static_cast<std::uint32_t>(
        std::ranges::find(kinds, object_kind::verbatim) - kinds.begin())};
    const auto row = container(t, verbatim);
    CHECK(t.elements.kinds[row] == element_kind::table_row);
    const auto cells = own_objects(t, row);
    CHECK(std::ranges::find(cells, verbatim) == cells.end());
    CHECK(cells.back() < verbatim);
    CHECK(verbatim < t.objects.subtree_ends[cells.back()]);
}

TEST_CASE("every link gives its type, path, description and heading",
    "[types][tables]") {
    const auto b = example();
    const auto& t = b.t;

    REQUIRE(t.links.size() == 1);
    const link_id l{0};
    const auto o = t.links.objects[l];
    CHECK(t.objects.kinds[o] == object_kind::link);
    CHECK(b.name_text(t.links.types[l]) == "id");
    CHECK(t.links.formats[l] == link_format::bracket);
    CHECK(t.links.paths[l].of(source) == "F1C69FBB");
    CHECK(t.objects.contents[o].of(source) == "the story");

    const auto paragraph = container(t, o);
    CHECK(t.elements.kinds[paragraph] == element_kind::paragraph);
    const auto heading = enclosing_heading(t, paragraph);
    REQUIRE(heading);
    CHECK(t.headings.titles[*heading].of(source) == "Goal");
}

TEST_CASE("plain text is the source between the objects of a paragraph",
    "[types][tables]") {
    const auto b = example();
    const auto& t = b.t;
    const auto goal = heading_titled(t, "Goal");
    REQUIRE(goal);
    const auto section = child_of_kind(t, t.headings.elements[*goal],
        element_kind::section);
    REQUIRE(section);
    const auto paragraph =
        child_of_kind(t, *section, element_kind::paragraph);
    REQUIRE(paragraph);

    std::vector<std::string> pieces;
    auto at = t.elements.contents[*paragraph].begin;
    for (const auto o : own_objects(t, *paragraph)) {
        const auto range = t.objects.ranges[o];
        pieces.emplace_back(source_range{at, range.begin}.of(source));
        pieces.emplace_back(range.of(source));
        at = range.end;
    }
    pieces.emplace_back(
        source_range{at, t.elements.contents[*paragraph].end}.of(source));

    CHECK(pieces == std::vector<std::string>{"Name the ", "*tables*",
        "; see ", "[[id:F1C69FBB][the story]]", "."});
}

TEST_CASE("a heading carries its keyword, tags and level",
    "[types][tables]") {
    const auto b = example();
    const auto& t = b.t;
    const auto status = heading_titled(t, "Status");
    REQUIRE(status);

    CHECK(t.headings.levels[*status] == 1);
    CHECK(b.name_text(t.headings.todos[*status]) == "STARTED");
    CHECK(t.headings.todo_types[*status] == todo_type::todo);
    CHECK(t.headings.tag_firsts[*status] == tag_id{0});
    REQUIRE(t.heading_tags.size() == 1);
    CHECK(b.name_text(t.heading_tags.names[tag_id{0}]) == "work");

    const auto goal = heading_titled(t, "Goal");
    REQUIRE(goal);
    CHECK(t.headings.todos[*goal].is_none());
    CHECK(t.headings.tag_firsts[*goal] == t.headings.tag_firsts[*status]);
}

TEST_CASE("a source block gives its language and body", "[types][tables]") {
    const auto b = example();
    const auto& t = b.t;

    REQUIRE(t.blocks.size() == 1);
    const block_id src{0};
    CHECK(t.elements.kinds[t.blocks.elements[src]] ==
        element_kind::src_block);
    CHECK(b.name_text(t.blocks.names[src]) == "sh");
    CHECK(t.blocks.bodies[src].of(source) == "./compass.sh lint\n");
    CHECK(t.blocks.parameters[src].empty());
}

TEST_CASE("the element rows are in pre-order", "[types][tables]") {
    const auto b = example();
    const auto& t = b.t;
    const auto& e = t.elements;

    REQUIRE(e.size() == 16);
    CHECK(e.kinds[element_id{0}] == element_kind::document);
    CHECK(e.parents[element_id{0}].is_none());
    CHECK(e.subtree_ends[element_id{0}].value == e.size());

    for (std::uint32_t i = 1; i < e.size(); ++i) {
        const element_id row{i};
        const auto parent = e.parents[row];
        INFO("element " << i);
        CHECK(parent < row);
        CHECK(row < e.subtree_ends[row]);
        CHECK(e.subtree_ends[row] <= e.subtree_ends[parent]);
        CHECK(e.ranges[parent].begin <= e.ranges[row].begin);
        CHECK(e.ranges[row].end <= e.ranges[parent].end);
        CHECK(e.object_firsts[element_id{i - 1}] <= e.object_firsts[row]);
    }

    CHECK(children(t, element_id{0}).size() == 3);
    CHECK(std::ranges::is_sorted(t.headings.elements.values));
    CHECK(std::ranges::is_sorted(t.keywords.elements.values));
    CHECK(std::ranges::is_sorted(t.links.objects.values));
}
