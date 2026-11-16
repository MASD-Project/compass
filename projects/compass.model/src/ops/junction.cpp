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
#include <string>
#include "entity.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

std::vector<std::string> split_underscores(std::string_view s) {
    std::vector<std::string> out;
    std::size_t start = 0;
    for (;;) {
        const auto end = s.find('_', start);
        out.emplace_back(s.substr(start, end == std::string_view::npos
            ? std::string_view::npos : end - start));
        if (end == std::string_view::npos)
            return out;
        start = end + 1;
    }
}

/**
 * @brief _fk_side_from_section.
 */
void fk_side(load_context& c, value_id out, node_id n) {
    auto& b = c.b;
    set_properties(b, out, c.o, n);
    const auto holder = b.detached_object();
    column_node(c, holder, n);
    for (const auto key : {"description", "detail", "generator_expr"}) {
        if (const auto v = b.find(holder, key); !v.is_none())
            b.set_copy(out, key, v);
    }
    const auto cpp_type = b.find(out, "cpp_type");
    const auto t = cpp_type.is_none() ? std::string() : text_value(b, cpp_type);
    const bool uuid = t == "boost::uuids::uuid";
    const bool date = t == "std::chrono::year_month_day";
    const bool timestamp = t == "std::chrono::system_clock::time_point";
    b.set(out, "is_uuid", uuid);
    b.set(out, "is_date", date);
    b.set(out, "is_timestamp", timestamp);
    b.set(out, "query_needs_str", uuid || date || timestamp);
}

void entity_names(builder& b, value_id j) {
    const auto singular = text_value(b, b.find(j, "name_singular"));
    const auto words = split_underscores(singular);
    b.set(j, "entity_singular", singular);
    if (const auto name = b.find(j, "name"); name.is_none())
        b.set(j, "entity_plural", singular + "s");
    else
        b.set_copy(j, "entity_plural", name);
    if (const auto title = b.find(j, "name_title"); title.is_none())
        b.set_null(j, "entity_title");
    else
        b.set_copy(j, "entity_title", title);
    if (const auto w = b.find(j, "name_singular_words"); truthy(b, w))
        b.set_copy(j, "entity_singular_words", w);
    else
        b.set(j, "entity_singular_words", words.back());
    b.set(j, "entity_snake", singular);
    std::string upper = singular;
    for (auto& ch : upper) {
        if (ch >= 'a' && ch <= 'z')
            ch = static_cast<char>(ch - 32);
    }
    b.set(j, "entity_upper", upper);
    std::string pascal;
    for (const auto& w : words)
        pascal += pascal_case(w);
    b.set(j, "entity_pascal", pascal);
    b.set(j, "entity_pascal_short", pascal_case(words.back()));
    const auto plural = text_value(b, b.find(j, "entity_plural"));
    const auto plural_words = split_underscores(plural);
    b.set(j, "entity_pascal_short_plural", pascal_case(plural_words.back()));
    b.set(j, "entity_plural_words", plural_words.back());
    const auto repo = b.find(j, "repository");
    if (!truthy(b, repo))
        return;
    const auto either = [&](std::string_view key, std::string_view from,
        value_id fallback_value, const std::string& fallback) {
        if (const auto v = b.find(repo, from); !v.is_none())
            b.set_copy(repo, key, v);
        else if (!fallback_value.is_none())
            b.set_copy(repo, key, fallback_value);
        else
            b.set(repo, key, fallback);
    };
    either("entity_singular_short", "name_singular_short", value_id::none(),
        singular);
    either("entity_plural_short", "name_short", b.find(j, "entity_plural"), "");
    either("entity_singular_words", "name_singular_words",
        b.find(j, "entity_singular_words"), "");
    either("entity_plural_words", "name_words",
        b.find(j, "entity_plural_words"), "");
}

}

void load_junction(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    const node_id document{0};
    ensure_profile_binding(o);
    const auto j = b.object(root, "junction");
    for (const auto key : {"product", "schema", "component", "name",
        "name_singular", "name_title", "name_singular_words", "brief",
        "http_area", "web_resource", "web_list_by"}) {
        if (const auto r = frontmatter(o, key); r.found)
            b.set(j, key, r.value);
    }
    for (const auto key : {"has_tenant_id", "web_read_only"}) {
        if (const auto r = frontmatter(o, key); r.found)
            set_typed(b, j, key, r.value);
    }
    if (const auto flags = section(o, document, "Flags"); !flags.is_none())
        set_properties(b, j, o, flags);
    if (!truthy(b, b.find(j, "name_singular")))
        throw model_error("a junction must declare :name_singular:");
    if (const auto text = body(o, document); !text.empty())
        b.set(j, "description", text);
    if (const auto left = section(o, document, "Left"); !left.is_none())
        fk_side(c, b.object(j, "left"), left);
    if (const auto right = section(o, document, "Right"); !right.is_none())
        fk_side(c, b.object(j, "right"), right);

    const auto columns = b.list(j, "columns");
    for (const auto n : children(o, section(o, document, "Columns")))
        column_node(c, b.append_object(columns), n);

    const auto fk_section = section(o, document, "Foreign keys");
    if (!children(o, fk_section).empty()) {
        const auto list = b.list(j, "foreign_keys");
        for (const auto n : children(o, fk_section))
            soft_fk_node(c, b.append_object(list), n);
    }
    const auto insert = section(o, document, "Insert trigger");
    if (const auto validations = section(o, insert, "Validations");
        !validations.is_none()) {
        const auto list = b.list(b.object(j, "insert_trigger"), "validations");
        for (const auto& r : first_table_rows(o, validations)) {
            const auto item = b.append_object(list);
            for (const auto& cell : r.cells)
                b.set(item, cell.key, cell.value);
        }
    }
    const auto sql = section(o, document, "SQL");
    if (const auto flags = section(o, sql, "Flags"); !flags.is_none())
        set_properties(b, b.object(j, "sql"), o, flags);
    if (const auto repo = section(o, document, "Repository"); !repo.is_none())
        set_properties(b, b.object(j, "repository"), o, repo);

    const auto cpp = section(o, document, "C++");
    if (const auto flags = section(o, cpp, "Flags"); !flags.is_none())
        set_properties(b, j, o, flags);
    const auto cpp_out = b.detached_object();
    if (!cpp.is_none()) {
        cpp_includes(c, cpp_out, section(o, cpp, "Domain includes"),
            section(o, cpp, "Entity includes"), columns);
        if (const auto conv = section(o, cpp, "Conventions"); !conv.is_none())
            set_properties(b, cpp_out, o, conv);
        if (const auto td = section(o, cpp, "Table display"); !td.is_none()) {
            const auto list = b.list(cpp_out, "table_display");
            for (const auto& r : own_table_rows(o, td)) {
                const auto item = b.append_object(list);
                b.set(item, "column", r.value_or("column"));
                b.set(item, "header", r.value_or("header"));
            }
        }
        if (const auto drawer = section(o, cpp, "Presentation");
            !drawer.is_none()) {
            presentation_drawer(c, b.object(j, "presentation"), drawer,
                b.find(j, "profile"));
            entity_names(b, j);
        }
    }
    if (truthy(b, cpp_out))
        b.set_copy(j, "cpp", cpp_out);

    collect_implementations(c, j, "implementations");
    apply_profile(c, j);
    b.set(j, "wire_write_enabled", !(truthy(b, b.find(j, "read_only")) ||
        truthy(b, b.find(j, "client_read_only"))));

    const auto ac = artefact_columns(c);
    std::vector<std::string> staged;
    if (!ac.node.is_none()) {
        const auto list = b.list(j, "artefact_columns");
        for (const auto n : ac.columns) {
            const auto column = b.append_object(list);
            column_node(c, column, n);
            staged.push_back(text_value(b, b.find(column, "name")));
        }
        b.set(j, "artefact_key_column", ac.key);
        if (!ac.checks.empty())
            throw model_error("junction artefact columns declare checks");
    }
    for (const auto side : {"left", "right"}) {
        bool indexed = ac.node.is_none();
        if (!indexed) {
            const auto s = b.find(j, side);
            if (s.is_none() || b.find(s, "column").is_none())
                throw model_error("junction side has no column");
            indexed = std::ranges::find(staged,
                text_value(b, b.find(s, "column"))) != staged.end();
        }
        b.set(j, "artefact_" + std::string(side) + "_indexed", indexed);
    }
}

}
