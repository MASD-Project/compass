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
#include "compass.crosscheck/codecs/convention_kind_terms.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codegen/ops/values.hpp"
#include "compass.crosscheck/ops/python_literals.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace mt = compass::model::types;
namespace pt = compass::platform::text;
namespace values = compass::codegen::ops::values;

constexpr std::string_view script_path =
    "projects/ores.sql/utility/generate_convention_kind_terms.py";
constexpr std::string_view terms_path =
    "projects/ores.sql/utility/convention_terms.json";
constexpr std::string_view mappers = "projects/ores.ore/core/src/domain/";
constexpr std::string_view out_path =
    "projects/ores.sql/populate/refdata/refdata_convention_kind_terms_populate.sql";
constexpr std::string_view kinds_path =
    "projects/ores.sql/populate/refdata/refdata_convention_kinds_populate.sql";
constexpr std::string_view triggers_path =
    "projects/ores.sql/create/refdata/refdata_convention_terms_validate_create.sql";
constexpr std::string_view drops_path =
    "projects/ores.sql/drop/refdata/refdata_convention_terms_validate_drop.sql";

constexpr std::string_view licence = R"(/* -*- sql-product: postgres; tab-width: 4; indent-tabs-mode: nil -*-
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
)";

constexpr std::string_view trigger_function = R"SQL(
/**
 * Refuses a convention value outside the value set of the term it holds, and
 * stores the form the set names for one it accepts.
 *
 * One enforcement point, in the database, as for a pricing parameter: the shell,
 * the screens and a data quality publish all write through it. The trigger names
 * the entity it guards; the map's child names each of that entity's terms, and the
 * map row for that term's address names its value set. Every non-empty value is
 * checked by ores_reporting_value_in_set_fn, the rule a pricing parameter's value
 * meets too. A term the map does not name is not checked.
 *
 * The canonical form of a value the set states is written back over the term, so a
 * document that says MODFOLLOWING and one that says ModifiedFollowing leave one
 * row. The row is rewritten through its JSON form because a trigger cannot name a
 * column it read from a table.
 */
create or replace function ores_refdata_validate_convention_terms_fn()
returns trigger
security definer
set search_path = public, pg_temp
as $$
declare
    v_row jsonb := to_jsonb(NEW);
    v_term record;
    v_value text;
    v_canonical text;
    v_stored jsonb := '{}'::jsonb;
begin
    for v_term in
        select k.column_name, m.value_set_code
          from ores_reporting_ore_element_map_columns_tbl k
          join ores_reporting_ore_parameter_vocabularies_tbl m
            on m.code = k.element_map_code
           and m.tenant_id = ores_utility_system_tenant_id_fn()
           and m.valid_to = ores_utility_infinity_timestamp_fn()
         where k.entity_name = TG_ARGV[0]
           and k.tenant_id = ores_utility_system_tenant_id_fn()
           and k.valid_to = ores_utility_infinity_timestamp_fn()
    loop
        v_value := v_row ->> v_term.column_name;
        if v_value is null or v_value = '' then
            continue;
        end if;
        if not ores_reporting_value_in_set_fn(v_term.value_set_code, v_value) then
            raise exception 'Invalid value % for %.%: the value is not in set %.',
                v_value, TG_ARGV[0], v_term.column_name, v_term.value_set_code;
        end if;
        v_canonical := ores_reporting_canonical_value_fn(v_term.value_set_code, v_value);
        if v_canonical is distinct from v_value then
            v_stored := v_stored || jsonb_build_object(v_term.column_name, v_canonical);
        end if;
    end loop;
    if v_stored <> '{}'::jsonb then
        NEW := jsonb_populate_record(NEW, v_stored);
    end if;
    return NEW;
end;
$$ language plpgsql;
)SQL";

/**
 * @brief Where the script stops without writing.
 */
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

struct row {
    std::string code;
    std::string kind;
    std::string entity;
    std::string column;
    std::string xpath;
    int position = 0;
};

std::string q(std::string_view value) {
    std::string out = "'";
    for (const char c : value) {
        if (c == '\'')
            out.push_back('\'');
        out.push_back(c);
    }
    return out + "'";
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

std::string render(const std::vector<row>& rows, const tables& t) {
    std::vector<std::string> values;
    for (const auto& r : rows)
        values.push_back("    (" + q(r.code) +
            ", ores_utility_system_tenant_id_fn(), 0, " + q(r.kind) + ", " +
            q(r.entity) + ", " + q(r.column) + ", " + q(t.schema_name) + ", " +
            q(r.xpath) + ", " + std::to_string(r.position) +
            ", 'ores_refdata_service', 'system.initial_load', "
            "'Seed convention kind terms')");
    return std::string(licence) + R"SQL(
/**
 * Convention Kind Terms Population Script
 *
 * The terms of each convention kind: the entity and the column a value travels
 * under, and the leaf of ORE's conventions document that holds it, which names
 * the term's entry in the ORE parameter contract. Generated by
 * projects/ores.sql/utility/generate_convention_kind_terms.py from the ores.ore
 * mapper and convention_terms.json. Do not edit it by hand. This script is
 * idempotent.
 */

\echo '--- Convention Kind Terms ---'

insert into ores_refdata_convention_kind_terms_tbl (
    code, tenant_id, version, kind_code, entity_name, column_name, schema_name, xpath,
    position, modified_by, change_reason_code, change_commentary
) values
)SQL" + join(values, ",\n") + R"SQL(
on conflict (tenant_id, code)
where valid_to = ores_utility_infinity_timestamp_fn()
do nothing;

select 'Convention Kind Terms' as entity, count(*) as count
from ores_refdata_convention_kind_terms_tbl
where valid_to = ores_utility_infinity_timestamp_fn();
)SQL";
}

std::string table_of(const ct::corpus_view& v, std::string_view entity) {
    static const pt::regex tablename(R"(^:tablename:\s+(\w+)\s*$)",
        {.multiline = true});
    const auto text = read(v, "projects/ores.refdata/modeling/ores.refdata." +
        std::string(entity) + ".org");
    const auto m = tablename.search(text);
    if (!m)
        throw stop{};
    return std::string(m->group(1));
}

void run(const ct::corpus_view& v, ct::file_table& out) {
    try {
        const auto t = read_tables(read(v, script_path));
        const auto m = mapper_terms(v, t);
        const auto known = xpaths(v, m.elements);
        if (m.unread)
            return;
        std::vector<row> rows;
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
                    return;
            }
            auto& p = position[f.kind];
            rows.push_back({code, f.kind, f.entity, f.column, xpath, p++});
        }
        std::set<std::string> kinds;
        for (const auto& r : rows)
            kinds.insert(r.kind);
        if (kind_problems(v, t, kinds))
            return;
        std::map<std::string, std::set<std::string>> columns;
        for (const auto& r : rows)
            columns[r.entity].insert(r.column);
        std::string triggers, drops;
        for (const auto& [entity, cols] : columns) {
            const auto table = table_of(v, entity);
            const auto stem = table.substr(0, table.size() < 4 ? 0
                : table.size() - 4);
            triggers += "\ncreate or replace trigger " + stem +
                "_terms_trg\nbefore insert or update of " +
                join({cols.begin(), cols.end()}, ", ") + "\non " + table +
                "\nfor each row execute function "
                "ores_refdata_validate_convention_terms_fn('" + entity +
                "');\n";
            drops += "drop trigger if exists " + stem + "_terms_trg on " +
                table + ";\n";
        }
        out.add(out_path, render(rows, t));
        out.add(triggers_path, std::string(licence) + R"SQL(
/**
 * Convention Term Validation
 *
 * The trigger function that checks a convention value against its term's value
 * set, and its trigger on every entity that holds a term. Generated by
 * projects/ores.sql/utility/generate_convention_kind_terms.py from the same
 * terms as the convention kind term seed. Do not edit it by hand.
 */
)SQL" + std::string(trigger_function) + triggers);
        out.add(drops_path, std::string(licence) + R"SQL(
/**
 * Drops the convention term validation triggers and their function. Generated by
 * projects/ores.sql/utility/generate_convention_kind_terms.py.
 */

)SQL" + drops + "drop function if exists "
            "ores_refdata_validate_convention_terms_fn();\n");
    } catch (const stop&) {
    }
}

}

const ct::descriptor convention_kind_terms{
    .name = "convention_kind_terms",
    .version = 1,
    .summary = "Writes the convention kind terms seed, their validation "
               "triggers and the triggers' drops from the ores.ore mapper.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = {},
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_table>{run}};

}
