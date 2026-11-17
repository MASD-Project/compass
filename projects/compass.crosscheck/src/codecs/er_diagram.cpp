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
#include "compass.crosscheck/codecs/er_diagram.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codegen/ops/compile.hpp"
#include "compass.codegen/ops/render.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.crosscheck/ops/python_literals.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.sql/ops/er_model.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace mo = compass::model::ops;
namespace mt = compass::model::types;
namespace so = compass::sql::ops;
namespace pt = compass::platform::text;

constexpr std::array<ct::option, 5> options{{
    {"create_dir", "projects/ores.sql/create",
        "The directory whose *_create.sql scripts the model is read from."},
    {"parser", "projects/ores.codegen/src/plantuml_er_parse_sql.py",
        "The parser whose COMPONENT_PREFIXES, FK_COLUMNS and TABLE_STYLES "
        "the model uses."},
    {"template", "projects/ores.codegen/library/templates/plantuml_er.mustache",
        "The template of one group's diagram."},
    {"index_template",
        "projects/ores.codegen/library/templates/plantuml_er_index.mustache",
        "The template of the index page."},
    {"output", "projects/ores.sql/modeling/ores_schema.puml",
        "The index page; each group's diagram is written beside it as "
        "<stem>.<group><suffix>."}}};

std::string text_of(std::string_view bytes) {
    return pt::universal_newlines(pt::decode_replacing(bytes));
}

const ops::python_value* entry(const ops::python_value& dict,
    std::string_view key) {
    for (const auto& e : dict.entries) {
        if (e.key == key)
            return &e.value;
    }
    return nullptr;
}

std::optional<std::string> string_entry(const ops::python_value& dict,
    std::string_view key) {
    const auto* v = entry(dict, key);
    if (v == nullptr || v->what != ops::python_value::kind::string)
        return std::nullopt;
    return v->text;
}

/**
 * @brief The parser's tables, or nothing when one is not a literal of the
 * shape the parser reads.
 */
std::optional<so::er_conventions> conventions_of(std::string_view source) {
    using kind = ops::python_value::kind;
    const auto prefixes = ops::python_assignment(source, "COMPONENT_PREFIXES");
    const auto keys = ops::python_assignment(source, "FK_COLUMNS");
    const auto styles = ops::python_assignment(source, "TABLE_STYLES");
    if (!prefixes || !keys || !styles || prefixes->what != kind::dict ||
        styles->what != kind::dict)
        return std::nullopt;
    so::er_conventions out;
    for (const auto& e : prefixes->entries) {
        const auto name = string_entry(e.value, "name");
        const auto description = string_entry(e.value, "description");
        const auto color = string_entry(e.value, "color");
        const auto* order = entry(e.value, "order");
        double rank = 99;
        if (!name || !description || !color)
            return std::nullopt;
        if (order != nullptr) {
            if (order->what != kind::number)
                return std::nullopt;
            const auto& t = order->text;
            if (std::from_chars(t.data(), t.data() + t.size(), rank).ec !=
                std::errc{})
                return std::nullopt;
        }
        out.components.push_back({e.key, *name, *description, *color, rank});
    }
    const auto columns = ops::strings_of(*keys);
    if (!columns)
        return std::nullopt;
    out.key_columns.insert(columns->begin(), columns->end());
    for (const auto& e : styles->entries) {
        out.styles.push_back({e.key, string_entry(e.value, "stereotype")
            .value_or(""), string_entry(e.value, "color").value_or("#FFFFFF")});
    }
    return out;
}

/**
 * @brief The values pystache renders: one table of every context.
 */
class contexts {
public:
    contexts() : b_(tables_) {}

    [[nodiscard]] const mt::model_tables& tables() const noexcept {
        return tables_;
    }

    mt::value_id object() { return b_.detached_object(); }

    void package(mt::value_id list, const so::er_package& p) {
        const auto o = b_.append_object(list);
        fill(o, p);
    }

    void fill(mt::value_id o, const so::er_package& p) {
        b_.set(o, "name", p.name);
        b_.set(o, "description", p.description);
        b_.set(o, "color", p.color);
        const auto tables = b_.list(o, "tables");
        for (const auto& t : p.tables)
            table(tables, t);
    }

    void relationship(mt::value_id list, const so::er_relationship& r) {
        const auto o = b_.append_object(list);
        b_.set(o, "from_table", r.from_table);
        b_.set(o, "to_table", r.to_table);
        b_.set(o, "cardinality", r.cardinality);
        b_.set(o, "label", r.label);
    }

    mo::builder& builder() noexcept { return b_; }

private:
    void table(mt::value_id list, const so::er_table& t) {
        const auto o = b_.append_object(list);
        b_.set(o, "name", t.name);
        b_.set(o, "stereotype", t.stereotype);
        b_.set(o, "color", t.color);
        const auto key = b_.list(b_.object(o, "primary_key"), "columns");
        for (const auto& k : t.key) {
            const auto c = b_.append_object(key);
            b_.set(c, "name", k.name);
            b_.set(c, "type", k.type);
            if (k.is_fk)
                b_.set(c, "is_fk", true);
        }
        const auto columns = b_.list(o, "columns");
        for (const auto& col : t.columns) {
            const auto c = b_.append_object(columns);
            b_.set(c, "name", col.name);
            b_.set(c, "type", col.type);
            b_.set(c, "nullable", col.nullable);
            b_.set(c, "is_pk", col.is_pk);
            b_.set(c, "is_fk", col.is_fk);
            b_.set(c, "is_unique", col.is_unique);
        }
        b_.set(o, "description", t.description);
    }

    mt::model_tables tables_;
    mo::builder b_;
};

std::string rendered(const compass::codegen::types::compiled_template& t,
    const contexts& c, mt::value_id context) {
    std::string out;
    compass::codegen::ops::render(out, t, c.tables(), context);
    return out;
}

void run(const ct::corpus_view& v, ct::file_output& out) {
    const auto parser = v.read_file(v.option("parser"));
    const auto group_source = v.read_file(v.option("template"));
    const auto index_source = v.read_file(v.option("index_template"));
    const auto unread = !parser ? "parser" : !group_source ? "template"
        : !index_source ? "index_template" : "";
    if (*unread != '\0') {
        co::refuse(out, v.option("output"), "cannot read " +
            std::string(v.option(unread)) + ", the option " + unread,
            "Restore the file, or set the option to the file that holds it.");
        return;
    }
    const auto conventions = conventions_of(text_of(*parser));
    if (!conventions) {
        co::refuse(out, v.option("output"), std::string(v.option("parser")) +
            " does not state COMPONENT_PREFIXES, FK_COLUMNS and TABLE_STYLES "
            "in the shape the parser reads", "Correct the literals of the "
            "parser.");
        return;
    }

    auto paths = v.list_files(std::string(v.option("create_dir")) + "/",
        "_create.sql");
    std::ranges::sort(paths, compass::cpp::ops::component_less);
    std::vector<std::string> texts;
    texts.reserve(paths.size());
    std::vector<so::er_script> scripts;
    for (const auto p : paths) {
        const auto bytes = v.read_file(p);
        if (!bytes)
            continue;
        texts.push_back(text_of(*bytes));
        scripts.push_back({p, texts.back()});
    }
    const auto model = so::er_model_of(scripts, *conventions);

    const std::string output(v.option("output"));
    const auto slash = output.rfind('/');
    const auto directory = slash == std::string::npos ? std::string()
        : output.substr(0, slash + 1);
    const auto file = output.substr(directory.size());
    const auto dot = file.rfind('.');
    const auto stem = dot == std::string::npos || dot == 0 ? file
        : file.substr(0, dot);
    const auto suffix = file.substr(stem.size());
    const auto group_file = [&](const std::string& name) {
        return stem + "." + name + suffix;
    };

    const auto group = compass::codegen::ops::compile(text_of(*group_source));
    const auto index = compass::codegen::ops::compile(text_of(*index_source));

    std::map<std::string_view, std::string_view> home;
    for (const auto& p : model.packages) {
        for (const auto& t : p.tables)
            home.emplace(t.name, p.name);
    }
    for (const auto& p : model.packages) {
        const auto inside = [&](std::string_view table) {
            return std::ranges::any_of(p.tables, [&](const auto& t) {
                return t.name == table;
            });
        };
        contexts c;
        auto& b = c.builder();
        const auto o = c.object();
        b.set(o, "diagram_title", p.name);
        b.set_integer(o, "group_table_count",
            static_cast<std::int64_t>(p.tables.size()));
        b.set(o, "index_file", file);
        b.set(o, "source_file", group_file(p.name));
        c.package(b.list(o, "packages"), p);
        const auto relationships = b.list(o, "relationships");
        std::map<std::string_view, std::string_view> crossing;
        for (const auto& r : model.relationships) {
            if (inside(r.from_table) && inside(r.to_table))
                c.relationship(relationships, r);
            using ends = std::pair<std::string_view, std::string_view>;
            for (const auto& [near, far] : {ends{r.from_table, r.to_table},
                     ends{r.to_table, r.from_table}}) {
                if (inside(near) && !inside(far)) {
                    const auto h = home.find(far);
                    crossing[far] = h == home.end() ? "unknown" : h->second;
                }
            }
        }
        std::string note;
        for (const auto& [table, owner] : crossing)
            note += (note.empty() ? "" : "\n") + std::string("- ") +
                std::string(table) + " (" + std::string(owner) + ")";
        b.set(o, "external_note", note);
        out.files.add(directory + group_file(p.name), rendered(group, c, o));
    }

    contexts c;
    auto& b = c.builder();
    const auto o = c.object();
    std::size_t total = 0;
    const auto packages = b.list(o, "packages");
    for (const auto& p : model.packages) {
        total += p.tables.size();
        const auto e = b.append_object(packages);
        c.fill(e, p);
        b.set_integer(e, "table_count", static_cast<std::int64_t>(
            p.tables.size()));
        b.set(e, "file", group_file(p.name));
    }
    b.set(o, "source_file", file);
    b.set_integer(o, "total_tables", static_cast<std::int64_t>(total));
    b.set_integer(o, "package_count", static_cast<std::int64_t>(
        model.packages.size()));
    out.files.add(output, rendered(index, c, o));
}

}

const ct::descriptor er_diagram{
    .name = "er_diagram",
    .version = 1,
    .summary = "Writes the PlantUML ER diagrams of the schema, one per table "
        "group and an index, from the create scripts.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = co::refusal_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{run}};

}
