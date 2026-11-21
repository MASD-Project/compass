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
#include "compass.crosscheck/ops/value_sets.hpp"

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.crosscheck/ops/projection_columns.hpp"

namespace compass::crosscheck::ops::value_sets {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;

constexpr std::string_view out_path = terms_path;
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

std::string render(const std::vector<ops::convention_term>& rows,
    std::string_view schema_name) {
    std::vector<std::string> values;
    for (const auto& r : rows)
        values.push_back("    (" + q(r.code) +
            ", ores_utility_system_tenant_id_fn(), 0, " + q(r.kind) + ", " +
            q(r.entity) + ", " + q(r.column) + ", " + q(schema_name) + ", " +
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

}

void write_convention_kind_terms(const ct::corpus_view& v,
    const std::optional<ops::convention_terms_result>& terms,
    ct::file_output& out) {
    if (!terms) {
        co::refuse(out, out_path, "the convention kind terms cannot be read "
            "from the ores.ore mapper and the convention models",
            "Run generate_convention_kind_terms.py, which names the "
            "problem, and correct the mapper or the curation it reads.");
        return;
    }
    const auto& rows = terms->rows;
    std::map<std::string, std::set<std::string>> columns;
    for (const auto& r : rows)
        columns[r.entity].insert(r.column);
    std::string triggers, drops;
    for (const auto& [entity, cols] : columns) {
        const auto table = ops::convention_table(v, entity);
        if (!table) {
            co::refuse(out, out_path, "the convention entity " + entity +
                " has no model with a :tablename:", "Give the entity's "
                "refdata model a :tablename: property.");
            return;
        }
        const auto stem = table->substr(0, table->size() < 4 ? 0
            : table->size() - 4);
        triggers += "\ncreate or replace trigger " + stem +
            "_terms_trg\nbefore insert or update of " +
            join({cols.begin(), cols.end()}, ", ") + "\non " + *table +
            "\nfor each row execute function "
            "ores_refdata_validate_convention_terms_fn('" + entity +
            "');\n";
        drops += "drop trigger if exists " + stem + "_terms_trg on " +
            *table + ";\n";
    }
    out.files.add(out_path, render(rows, terms->schema_name));
    out.files.add(triggers_path, std::string(licence) + R"SQL(
/**
 * Convention Term Validation
 *
 * The trigger function that checks a convention value against its term's value
 * set, and its trigger on every entity that holds a term. Generated by
 * projects/ores.sql/utility/generate_convention_kind_terms.py from the same
 * terms as the convention kind term seed. Do not edit it by hand.
 */
)SQL" + std::string(trigger_function) + triggers);
    out.files.add(drops_path, std::string(licence) + R"SQL(
/**
 * Drops the convention term validation triggers and their function. Generated by
 * projects/ores.sql/utility/generate_convention_kind_terms.py.
 */

)SQL" + drops + "drop function if exists "
        "ores_refdata_validate_convention_terms_fn();\n");
}

}
