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
#include "compass.crosscheck/ops/projection_columns.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include "compass.codegen/ops/values.hpp"
#include "compass.crosscheck/ops/curve_wiring.hpp"
#include "compass.crosscheck/ops/python_literals.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::ops {

namespace {

namespace ct = compass::codec::types;
namespace mt = compass::model::types;
namespace pt = compass::platform::text;
namespace values = compass::codegen::ops::values;
namespace cw = compass::crosscheck::ops::curve;

constexpr std::string_view script_path =
    "projects/ores.sql/utility/generate_convention_kind_terms.py";
constexpr std::string_view terms_path =
    "projects/ores.sql/utility/convention_terms.json";
constexpr std::string_view mappers = "projects/ores.ore/core/src/domain/";
constexpr std::string_view kinds_path =
    "projects/ores.sql/populate/refdata/refdata_convention_kinds_populate.sql";
constexpr std::string_view refdata_models =
    "projects/ores.refdata/modeling/ores.refdata.";

struct stop {};

using names = std::map<std::string, std::string, std::less<>>;
using triple = std::tuple<std::string, std::string, std::string>;

/**
 * @brief The tables the script keeps as module-level literals.
 */
struct tables {
    std::vector<std::string> asset_classes;
    std::set<triple> not_a_column;
    std::string schema_name;
    std::vector<std::pair<std::string, std::string>> file_kind;
    std::map<std::string, std::string, std::less<>> spelled_otherwise;
    std::set<std::pair<std::string, std::string>> read_elsewhere;
    std::vector<std::pair<std::string, std::string>> element_of;
    names member_entity;
};

ops::python_value assignment(std::string_view source, std::string_view name) {
    auto v = ops::python_assignment(source, name);
    if (!v)
        throw stop{};
    return std::move(*v);
}

std::vector<std::pair<std::string, std::string>> string_dict(
    const ops::python_value& v) {
    if (v.what != ops::python_value::kind::dict)
        throw stop{};
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& e : v.entries) {
        if (!e.key_items.empty() ||
            e.value.what != ops::python_value::kind::string)
            throw stop{};
        out.emplace_back(e.key, e.value.text);
    }
    return out;
}

std::vector<std::string> tuple_key(const ops::python_entry& e,
    std::size_t size) {
    const auto key = ops::strings_of(ops::python_value{
        ops::python_value::kind::sequence, {}, e.key_items, {}});
    if (!key || key->size() != size)
        throw stop{};
    return *key;
}

tables read_tables(std::string_view source) {
    tables t;
    const auto classes = ops::strings_of(assignment(source, "ASSET_CLASSES"));
    if (!classes)
        throw stop{};
    t.asset_classes = *classes;
    for (const auto& e : assignment(source, "NOT_A_COLUMN").entries) {
        const auto k = tuple_key(e, 3);
        t.not_a_column.emplace(k[0], k[1], k[2]);
    }
    t.schema_name = assignment(source, "SCHEMA_NAME").text;
    t.file_kind = string_dict(assignment(source, "FILE_KIND"));
    for (const auto& e : assignment(source, "SPELLED_OTHERWISE").entries) {
        std::optional<std::string> xpath;
        for (const auto& f : e.value.entries)
            if (f.key == "xpath")
                xpath = f.value.text;
        if (!xpath)
            throw stop{};
        t.spelled_otherwise.insert_or_assign(e.key, *xpath);
    }
    for (const auto& e : assignment(source, "READ_ELSEWHERE").entries) {
        const auto k = tuple_key(e, 2);
        t.read_elsewhere.emplace(k[0], k[1]);
    }
    t.element_of = string_dict(assignment(source, "ELEMENT_OF"));
    for (auto& [k, v] : string_dict(assignment(source, "MEMBER_ENTITY")))
        t.member_entity.insert_or_assign(k, v);
    return t;
}

template <typename T>
void put(std::vector<std::pair<std::string, T>>& v, std::string key,
    T value) {
    for (auto& [k, existing] : v)
        if (k == key) {
            existing = std::move(value);
            return;
        }
    v.emplace_back(std::move(key), std::move(value));
}

std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return out;
}

struct term {
    std::string kind;
    std::string entity;
    std::string column;
    std::vector<std::string> below;
};

struct mapped {
    std::vector<term> found;
    std::vector<std::pair<std::string, std::string>> elements;
    bool unread = false;
};

std::string read(const ct::corpus_view& v, std::string_view path) {
    auto text = v.read_file(path);
    if (!text)
        throw stop{};
    return std::move(*text);
}

mapped mapper_terms(const ct::corpus_view& v, const tables& t) {
    static const pt::regex function_re(
        R"(\n(?:refdata::domain::[\w:]+|[\w:]+)\s+conventions_mapper::map_\w+\([^)]*\)\s*\{(.*?)\n\})",
        {.dotall = true});
    static const pt::regex declared_re(R"(refdata::domain::(\w+)\s+(\w+)\s*[;{=])");
    static const pt::regex alias_re(R"(auto&\s+(\w+)\s*=\s*\w+\.(\w+)\s*;)");
    static const pt::regex child_re(
        R"re(auto\s+(\w+)\s*=\s*\*?(\w+)(?:->|\.)child\("(\w+)"\))re");
    static const pt::regex read_re(
        R"re((?:\b(\w+)(?:\.\w+)*\.(\w+)\s*=\s*\w+\(\s*\*?(\w+),\s*"(\w+)")re"
        R"re(|assign\(\s*(\w+)(?:\.\w+)*\.(\w+),\s*\w+\(\s*\*?(\w+),\s*"(\w+)"))re");
    static const pt::regex pack_re(
        R"re(pack_family\(\s*bundle,\s*"(\w+)",\s*root\.children\("(\w+)"\))re");
    mapped m;
    m.elements = t.element_of;
    std::vector<std::pair<std::string, std::string>> files;
    const std::string family = std::string(mappers) + "conventions_family_";
    for (const auto path : v.list_files(family, ".cpp")) {
        if (path.find('/', family.size()) != std::string_view::npos)
            continue;
        const auto name = path.substr(mappers.size());
        files.emplace_back(std::string(path), upper(name.substr(
            19, name.size() - 19 - 4)));
    }
    for (const auto& [name, kind] : t.file_kind)
        files.emplace_back(std::string(mappers) + name, kind);
    for (const auto& [path, kind] : files) {
        const auto text = read(v, path);
        for (const auto& p : pack_re.find_all(text))
            put(m.elements, upper(p.group(1)), std::string(p.group(2)));
        for (const auto& f : function_re.find_all(text)) {
            const auto body = f.group(1);
            names types;
            for (const auto& d : declared_re.find_all(body))
                types.insert_or_assign(std::string(d.group(2)),
                    std::string(d.group(1)));
            for (const auto& a : alias_re.find_all(body))
                if (const auto it = t.member_entity.find(a.group(2));
                    it != t.member_entity.end())
                    types.insert_or_assign(std::string(a.group(1)),
                        it->second);
            struct reader {
                std::size_t start;
                std::string name;
                std::vector<std::string> below;
            };
            std::vector<reader> readers{{0, "v", {}}};
            const auto path_of = [&](std::string_view name, std::size_t at)
                -> const std::vector<std::string>* {
                for (auto r = readers.rbegin(); r != readers.rend(); ++r)
                    if (r->name == name && r->start <= at)
                        return &r->below;
                return nullptr;
            };
            for (const auto& c : child_re.find_all(body)) {
                const auto* parent = path_of(c.group(2), c.start());
                if (parent == nullptr)
                    continue;
                auto below = *parent;
                below.emplace_back(c.group(3));
                readers.push_back({c.start(), std::string(c.group(1)),
                    std::move(below)});
                std::ranges::stable_sort(readers, {}, &reader::start);
            }
            for (const auto& r : read_re.find_all(body)) {
                const std::size_t g = r.matched(1) ? 1 : 5;
                const auto var = std::string(r.group(g));
                const auto column = std::string(r.group(g + 1));
                const auto* below = path_of(r.group(g + 2), r.start());
                if (t.not_a_column.contains({kind, var, column}))
                    continue;
                const auto type = types.find(var);
                if (type == types.end() || below == nullptr) {
                    m.unread = true;
                    continue;
                }
                auto leaf = *below;
                leaf.emplace_back(r.group(g + 3));
                m.found.push_back({kind, type->second, column,
                    std::move(leaf)});
            }
        }
    }
    return m;
}

std::map<std::string, std::set<std::string>, std::less<>> xpaths(
    const ct::corpus_view& v,
    const std::vector<std::pair<std::string, std::string>>& elements) {
    const auto json = read(v, terms_path);
    mt::model_tables tbl;
    compass::model::ops::builder b(tbl);
    mt::value_id root;
    try {
        root = compass::model::io::read_json(b, json);
    } catch (const std::runtime_error&) {
        throw stop{};
    }
    const auto extracted = values::member(tbl, root, "conventions");
    std::map<std::string, std::set<std::string>, std::less<>> out;
    for (const auto& [kind, element] : elements) {
        const auto convention = values::member(tbl, extracted, element);
        if (convention.is_none())
            continue;
        for (const auto term : values::items(tbl, convention, "terms")) {
            std::string leaf(values::str(tbl, term, "name"));
            if (values::truthy(tbl, term, "list_item"))
                leaf += "/" + std::string(values::str(tbl, term, "list_item"));
            std::string xpath = "/Conventions";
            for (const auto p : values::items(tbl, term, "path"))
                xpath += "/" + std::string(values::str(tbl, p));
            out[kind].insert(xpath + "/" + leaf);
        }
    }
    return out;
}

bool kind_problems(const ct::corpus_view& v, const tables& t,
    const std::set<std::string>& kinds_with_terms) {
    static const pt::regex row_re(
        R"(\(ores_utility_system_tenant_id_fn\(\), '([A-Z_]+)', 0, '[^']*',\n)"
        R"(\s+'[^']*',\n\s+'([^']*)', (null|'[^']*'),)");
    const auto text = read(v, kinds_path);
    const auto rows = row_re.find_all(text);
    if (rows.empty())
        return true;
    for (const auto& r : rows) {
        if (std::ranges::find(t.asset_classes, r.group(2)) ==
            t.asset_classes.end())
            return true;
        if (kinds_with_terms.contains(std::string(r.group(1))) &&
            r.group(3) == "null")
            return true;
    }
    return false;
}

std::string join(const std::vector<std::string>& parts, std::string_view sep) {
    std::string out;
    for (const auto& p : parts) {
        if (!out.empty())
            out += sep;
        out += p;
    }
    return out;
}

/**
 * @brief The :tablename: line of the entity's model, as
 * generate_curve_column_guards.py reads it.
 */
std::optional<std::string> curve_table(const ct::corpus_view& v,
    std::string_view entity) {
    const auto text = v.read_file(std::string(refdata_models) +
        std::string(entity) + ".org");
    if (!text)
        return std::nullopt;
    std::string_view rest(*text);
    while (!rest.empty()) {
        const auto end = rest.find('\n');
        auto line = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{}
            : rest.substr(end + 1);
        if (!line.starts_with(":tablename:"))
            continue;
        line.remove_prefix(line.find(':', 1) + 1);
        return std::string(values::strip(line));
    }
    return std::nullopt;
}

}

std::optional<convention_terms_result> convention_terms(
    const ct::corpus_view& v) {
    try {
        const auto t = read_tables(read(v, script_path));
        const auto m = mapper_terms(v, t);
        const auto known = xpaths(v, m.elements);
        if (m.unread)
            return std::nullopt;
        convention_terms_result out{t.schema_name, {}};
        std::set<std::string> seen;
        std::map<std::string, int> position;
        for (const auto& f : m.found) {
            const auto code = f.kind + ":" + f.entity + "." + f.column;
            if (!seen.insert(code).second)
                continue;
            if (t.read_elsewhere.contains({f.kind, f.below.front()}))
                continue;
            std::string element = "?";
            for (const auto& [k, e] : m.elements)
                if (k == f.kind)
                    element = e;
            auto xpath = "/Conventions/" + element + "/" + join(f.below, "/");
            if (const auto s = t.spelled_otherwise.find(code);
                s != t.spelled_otherwise.end()) {
                xpath = s->second;
            } else {
                const auto k = known.find(f.kind);
                if (k == known.end() || !k->second.contains(xpath))
                    return std::nullopt;
            }
            auto& p = position[f.kind];
            out.rows.push_back({code, f.kind, f.entity, f.column, xpath, p++});
        }
        std::set<std::string> kinds;
        for (const auto& r : out.rows)
            kinds.insert(r.kind);
        if (kind_problems(v, t, kinds))
            return std::nullopt;
        return out;
    } catch (const stop&) {
        return std::nullopt;
    }
}

std::optional<std::string> convention_table(const ct::corpus_view& v,
    std::string_view entity) {
    static const pt::regex tablename(R"(^:tablename:\s+(\w+)\s*$)",
        {.multiline = true});
    const auto text = v.read_file(std::string(refdata_models) +
        std::string(entity) + ".org");
    if (!text)
        return std::nullopt;
    const auto m = tablename.search(*text);
    if (!m)
        return std::nullopt;
    return std::string(m->group(1));
}

std::optional<std::vector<projection_column>> curve_columns(
    const ct::corpus_view& v) {
    const auto projection = v.read_file(cw::projection_path);
    const auto schema = v.read_file(cw::schema_path);
    if (!projection || !schema)
        return std::nullopt;
    const auto declared = cw::leaves_of_schema(*schema);
    if (!declared)
        return std::nullopt;
    const auto wired = cw::wiring(*projection, *declared);
    if (!wired || !wired->problems.empty())
        return std::nullopt;
    std::map<std::string, projection_column> found;
    std::map<std::string, std::string> entity_of;
    for (const auto& s : wired->sections)
        for (const auto& p : s.parameters) {
            if (p.entity == cw::definition_entity)
                continue;
            auto table = curve_table(v, p.entity);
            if (!table)
                return std::nullopt;
            if (entity_of.try_emplace(*table, p.entity).first->second !=
                p.entity)
                return std::nullopt;
            const auto code = p.entity + "|" + p.column;
            found.insert_or_assign(code, projection_column{code, p.entity,
                p.column, *table, "/" + p.xpath});
        }
    std::vector<projection_column> out;
    for (auto& [code, c] : found)
        out.push_back(std::move(c));
    return out;
}

}
