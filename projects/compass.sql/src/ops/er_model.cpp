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
#include "compass.sql/ops/er_model.hpp"

#include <algorithm>
#include <map>
#include <optional>
#include <utility>
#include "compass.platform/text/regex.hpp"

namespace compass::sql::ops {

namespace {

namespace pt = compass::platform::text;

constexpr std::string_view table_head =
    R"re(create\s+table\s+if\s+not\s+exists\s+)re"
    R"re((?:"?(?:public)"?\.)?"?(\w+)"?)re";

bool is_space(char c) {
    return c == ' ' || (c >= '\t' && c <= '\r') || (c >= '\x1c' && c <= '\x1f');
}

std::string_view strip(std::string_view s) {
    while (!s.empty() && is_space(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && is_space(s.back()))
        s.remove_suffix(1);
    return s;
}

std::string_view strip_quotes(std::string_view s) {
    while (!s.empty() && s.front() == '"')
        s.remove_prefix(1);
    while (!s.empty() && s.back() == '"')
        s.remove_suffix(1);
    return s;
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& c : out) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    }
    return out;
}

std::vector<std::string_view> split(std::string_view s, char by) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (true) {
        const auto end = s.find(by, start);
        if (end == std::string_view::npos) {
            out.push_back(s.substr(start));
            return out;
        }
        out.push_back(s.substr(start, end - start));
        start = end + 1;
    }
}

std::string_view before(std::string_view s, std::string_view separator) {
    return s.substr(0, s.find(separator));
}

std::string join(const std::vector<std::string>& lines, std::string_view by) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i)
        out += (i == 0 ? "" : std::string(by)) + lines[i];
    return out;
}

/**
 * @brief A table as the script parses it, before the markings that need
 * every script.
 */
struct parsed_table {
    std::string name;
    std::string path;
    std::uint32_t line = 0;
    std::string component;
    std::string entity;
    std::string classification;
    std::vector<er_column> columns;
    std::vector<std::string> key;
    std::string description;
};

/**
 * @brief What the script's parser keeps across scripts: the tables by
 * name in the order first parsed, the descriptions, the unique columns
 * and the foreign keys each table states, in the order stated.
 */
struct parser {
    const er_conventions& conventions;
    std::vector<parsed_table> tables;
    std::map<std::string, std::size_t, std::less<>> rows;
    std::map<std::string, std::string, std::less<>> descriptions;
    std::map<std::string, std::set<std::string, std::less<>>, std::less<>>
        unique_columns;
    std::vector<std::pair<std::string, std::vector<std::pair<std::string,
        std::string>>>> fk_targets;

    parsed_table* find(std::string_view name) {
        const auto it = rows.find(name);
        return it == rows.end() ? nullptr : &tables[it->second];
    }

    void set_fk_target(const std::string& table, const std::string& column,
        const std::string& target) {
        auto it = std::ranges::find(fk_targets, table,
            &decltype(fk_targets)::value_type::first);
        if (it == fk_targets.end()) {
            fk_targets.emplace_back(table, decltype(it->second){});
            it = std::prev(fk_targets.end());
        }
        auto& targets = it->second;
        const auto c = std::ranges::find(targets, column,
            &std::pair<std::string, std::string>::first);
        if (c == targets.end())
            targets.emplace_back(column, target);
        else
            c->second = target;
    }

    void extract_descriptions(std::string_view content) {
        static const pt::regex block(std::string(R"re(-- =+\s*\n)re"
            R"re(((?:-- [^\n]*\n)+))re" R"re(-- =+\s*\n)re" R"re(\s*)re") +
            std::string(table_head), {.ignorecase = true});
        static const pt::regex javadoc(std::string(R"re(/\*\*\s*\n)re"
            R"re(((?:\s*\*[^\n]*\n)+))re" R"re(\s*\*/\s*\n)re"
            R"re((?:set\s+schema[^\n]*\n)?)re" R"re(\s*)re"
            R"re((?:-- [^\n]*\n)*)re"
            R"re(\s*)re") + std::string(table_head), {.ignorecase = true});
        for (const auto& m : block.find_all(content)) {
            std::vector<std::string> lines;
            for (auto line : split(m.group(1), '\n')) {
                line = strip(line);
                if (!line.starts_with("-- "))
                    continue;
                const auto text = strip(line.substr(3));
                if (!text.empty() && !text.starts_with('=') &&
                    !text.starts_with("Table:"))
                    lines.emplace_back(text);
            }
            if (!lines.empty())
                descriptions[std::string(m.group(2))] = join(lines, "\n");
        }
        for (const auto& m : javadoc.find_all(content)) {
            const std::string name(m.group(2));
            if (descriptions.contains(name))
                continue;
            std::vector<std::string> lines;
            for (auto line : split(m.group(1), '\n')) {
                line = strip(line);
                if (!line.starts_with("* "))
                    continue;
                const auto text = strip(line.substr(2));
                if (!text.empty() && lines.empty() &&
                    (text.contains("Table") || text.contains("Artefact")))
                    continue;
                if (!text.empty())
                    lines.emplace_back(text);
            }
            if (!lines.empty())
                descriptions[name] = join(lines, "\n");
        }
    }

    std::string component_of(std::string_view name) const {
        for (const auto& c : conventions.components) {
            if (name.starts_with(c.prefix))
                return c.name;
        }
        return "unknown";
    }

    static std::vector<er_column> columns_of(std::string_view body) {
        static const pt::regex column(R"re("?(\w+)"?\s+)re"
            R"re(([\w\s]+?(?:\s*\([^)]*\))?(?:\[\])?))re"
            R"re((?:\s+(?:NOT\s+NULL|NULL|DEFAULT|PRIMARY|REFERENCES|)re"
            R"re(CHECK).*)?$)re",
            {.ignorecase = true});
        static constexpr std::array<std::string_view, 6> constraints{
            "PRIMARY KEY", "EXCLUDE", "CHECK", "FOREIGN KEY", "CONSTRAINT",
            "UNIQUE"};
        static constexpr std::array<std::pair<std::string_view,
            std::string_view>, 3> shorter{{
            {"timestamp with time zone", "timestamptz"},
            {"character varying", "varchar"},
            {"double precision", "double"}}};
        std::vector<er_column> out;
        for (auto line : split(body, '\n')) {
            line = strip(line);
            while (!line.empty() && line.back() == ',')
                line.remove_suffix(1);
            if (line.empty())
                continue;
            const auto head = upper(line);
            if (std::ranges::any_of(constraints, [&](auto k) {
                    return head.starts_with(k);
                }))
                continue;
            const auto m = column.match(line);
            if (!m)
                continue;
            auto type = lower(strip(m->group(2)));
            for (const auto& [from, to] : shorter) {
                if (type == from) {
                    type = to;
                    break;
                }
            }
            out.push_back({.name = std::string(m->group(1)),
                .type = std::move(type),
                .nullable = !lower(line).contains("not null")});
        }
        return out;
    }

    static std::string classification_of(const std::vector<er_column>& cols,
        std::string_view name) {
        const auto has = [&](std::string_view c) {
            return std::ranges::any_of(cols, [&](const auto& col) {
                return col.name == c;
            });
        };
        const bool temporal = has("valid_from") && has("valid_to");
        if (name.contains("_artefact_") || name.ends_with("_artefact_tbl"))
            return "artefact";
        if (temporal) {
            const auto ids = std::ranges::count_if(cols, [](const auto& c) {
                return c.name.ends_with("_id");
            });
            if (ids >= 2 && !has("id")) {
                const auto others = std::ranges::count_if(cols,
                    [](const auto& c) {
                        return !c.name.ends_with("_id") &&
                            c.name != "valid_from" && c.name != "valid_to" &&
                            c.name != "version";
                    });
                if (others <= 5)
                    return "junction";
            }
            return "temporal";
        }
        if (has("dataset_id"))
            return "artefact";
        return "current_state";
    }

    static std::vector<std::string> key_of(std::string_view body,
        std::vector<er_column>& columns) {
        static const pt::regex primary(R"re(primary\s+key\s*\(([^)]+)\))re",
            {.ignorecase = true});
        const auto m = primary.search(body);
        if (!m)
            return {};
        std::vector<std::string> named;
        for (const auto part : split(m->group(1), ','))
            named.emplace_back(strip_quotes(strip(part)));
        std::vector<std::string> out;
        for (auto& c : columns) {
            if (std::ranges::find(named, c.name) == named.end())
                continue;
            c.is_pk = true;
            out.push_back(c.name);
        }
        std::ranges::stable_sort(out, {}, [&](const std::string& n) {
            return std::ranges::find(named, n) - named.begin();
        });
        return out;
    }

    void extract_tables(std::string_view content, std::string_view path) {
        static const pt::regex table(std::string(table_head) +
            R"re(\s*\((.*?)\);)re", {.dotall = true, .ignorecase = true});
        for (const auto& m : table.find_all(content)) {
            parsed_table t;
            t.name = m.group(1);
            t.path = path;
            t.line = static_cast<std::uint32_t>(1 + std::ranges::count(
                content.substr(0, m.start()), '\n'));
            t.component = component_of(t.name);
            t.entity = t.name;
            for (const auto& c : conventions.components) {
                if (t.name.starts_with(c.prefix)) {
                    t.entity = t.name.substr(c.prefix.size());
                    break;
                }
            }
            if (t.entity.ends_with("_tbl"))
                t.entity.resize(t.entity.size() - 4);
            t.columns = columns_of(m.group(2));
            t.classification = classification_of(t.columns, t.name);
            t.key = key_of(m.group(2), t.columns);
            if (const auto d = descriptions.find(t.name);
                d != descriptions.end())
                t.description = d->second;
            if (auto* existing = find(t.name))
                *existing = std::move(t);
            else {
                rows.emplace(t.name, tables.size());
                tables.push_back(std::move(t));
            }
        }
    }

    void record_fk_targets(std::string_view content,
        const std::vector<std::string>& parsed_here) {
        static const pt::regex soft(R"re(--\s*Validate\s+(\w+)\s+)re"
            R"re(\((?:optional\s+)?soft FK to\s+([a-z_0-9]+)\))re");
        static const pt::regex reference(
            R"re(^\s*"?(\w+)"?\s+[^,\n]*?\breferences\s+)re"
            R"re("?([a-z_0-9]+_tbl)"?)re",
            {.multiline = true, .ignorecase = true});
        static const pt::regex constraint(
            R"re(\bforeign\s+key\s*\(\s*"?(\w+)"?)re"
            R"re(\s*\)\s*references\s+"?([a-z_0-9]+_tbl)"?)re",
            {.ignorecase = true});
        std::vector<std::pair<std::string, std::string>> stated;
        for (const auto* r : {&soft, &reference, &constraint}) {
            for (const auto& m : r->find_all(content))
                stated.emplace_back(m.group(1), m.group(2));
        }
        for (const auto& [column, target] : stated) {
            const std::string* owner = nullptr;
            std::size_t owners = 0;
            for (const auto& name : parsed_here) {
                const auto& cols = find(name)->columns;
                if (std::ranges::any_of(cols, [&](const auto& c) {
                        return c.name == column;
                    })) {
                    owner = &name;
                    ++owners;
                }
            }
            if (owners == 1)
                set_fk_target(*owner, column, target);
        }
    }

    void extract_unique_indexes(std::string_view content) {
        static const pt::regex index(
            R"re(create\s+unique\s+index\s+if\s+not\s+exists\s+\w+\s+)re"
            R"re(on\s+"?(?:public)"?\."?(\w+)"?\s*\(([^)]+)\))re",
            {.ignorecase = true});
        for (const auto& m : index.find_all(content)) {
            auto& unique = unique_columns[std::string(m.group(1))];
            const auto part = before(before(m.group(2), " where "),
                " WHERE ");
            for (const auto column : split(part, ',')) {
                const auto name = strip_quotes(strip(column));
                if (!name.empty() && !name.contains('('))
                    unique.emplace(name);
            }
        }
    }

    void parse(const er_script& s) {
        extract_descriptions(s.text);
        std::set<std::string, std::less<>> before_here;
        for (const auto& t : tables)
            before_here.insert(t.name);
        extract_tables(s.text, s.path);
        std::vector<std::string> parsed_here;
        for (const auto& t : tables) {
            if (!before_here.contains(t.name))
                parsed_here.push_back(t.name);
        }
        record_fk_targets(s.text, parsed_here);
        extract_unique_indexes(s.text);
    }

    void mark_columns() {
        for (auto& t : tables) {
            const auto unique = unique_columns.find(t.name);
            std::string singular = t.entity;
            while (!singular.empty() && singular.back() == 's')
                singular.pop_back();
            const std::array<std::string, 3> own{singular + "_id",
                t.entity + "_id", "id"};
            for (auto& c : t.columns) {
                if (unique != unique_columns.end() &&
                    unique->second.contains(c.name))
                    c.is_unique = true;
                if (std::ranges::find(own, c.name) != own.end())
                    continue;
                if (conventions.key_columns.contains(c.name)) {
                    if (c.name == "country_code" &&
                        t.name.contains("session"))
                        continue;
                    c.is_fk = true;
                } else if (c.name.ends_with("_id")) {
                    c.is_fk = true;
                }
            }
        }
    }

    static std::string label_of(std::string_view from, std::string_view to,
        std::string_view column);

    std::vector<er_relationship> relationships() {
        std::vector<er_relationship> out;
        for (const auto& [name, targets] : fk_targets) {
            auto& table = *find(name);
            for (const auto& [column, target] : targets) {
                if (find(target) == nullptr)
                    continue;
                if (target == name && column != "upstream_derivation_id")
                    continue;
                for (auto& c : table.columns) {
                    if (c.name == column)
                        c.is_fk = true;
                }
                std::string cardinality = "||--o{";
                std::string label;
                if (column == "account_id" && name.contains("login_info")) {
                    cardinality = "||--o|";
                    label = "has";
                } else if (name.contains("artefact") &&
                           column == "dataset_id") {
                    label = "contains";
                } else {
                    label = label_of(target, name, column);
                }
                if (std::ranges::none_of(out, [&](const auto& r) {
                        return r.from_table == target && r.to_table == name;
                    }))
                    out.push_back({target, name, cardinality, label});
            }
        }
        return out;
    }
};

std::string parser::label_of(std::string_view from, std::string_view to,
    std::string_view column) {
    if (column == "change_reason_code")
        return "reason for";
    if (column == "category_code" && from.contains("change_reason_categor"))
        return "contains";
    if (to.contains("session") && from.contains("account"))
        return "creates";
    if (to.contains("account_role")) {
        if (from.contains("account"))
            return "assigned";
        if (from.contains("role"))
            return "granted to";
    }
    if (to.contains("role_permission")) {
        if (from.contains("permission"))
            return "included in";
        if (from.contains("role"))
            return "has";
    }
    if (to.contains("image_tag")) {
        if (from.contains("image"))
            return "tagged with";
        if (from.contains("tag"))
            return "applied to";
    }
    if (to.contains("telemetry_log"))
        return "generates";
    if (from.contains("coding_scheme") && !from.contains("authority"))
        return "identifies";
    if (from.contains("authority_type"))
        return "classifies";
    if (from.contains("catalog") && to.contains("dataset"))
        return "groups";
    if (from.contains("data_domain")) {
        if (to.contains("subject_area"))
            return "contains";
        if (to.contains("dataset") || to.contains("coding_scheme"))
            return "groups";
    }
    if (from.contains("subject_area")) {
        if (to.contains("coding_scheme"))
            return "defines";
        if (to.contains("dataset"))
            return "classifies";
    }
    if (from.contains("methodolog"))
        return "applied to";
    if (from.contains("dimension")) {
        if (from.contains("nature"))
            return "describes nature";
        if (from.contains("origin"))
            return "describes origin";
        if (from.contains("treatment"))
            return "describes treatment";
    }
    if (from.contains("dataset") && to.contains("dependenc"))
        return "depends on";
    if (from.contains("dataset") && to.contains("publication"))
        return "published as";
    if (column == "upstream_derivation_id")
        return "derives from";
    return "has";
}

struct package_head {
    std::string name;
    std::string description;
    std::string color;
    double order;
};

/**
 * @brief The package of a data quality table, by the shape of its name.
 */
package_head dq_package_of(std::string_view table) {
    static constexpr std::array<std::string_view, 6> datasets{"datasets_tbl",
        "dataset_bundles_tbl", "dataset_bundle_members_tbl",
        "dataset_dependencies_tbl", "dataset_publications_tbl",
        "bundle_publications_tbl"};
    static constexpr std::array<std::string_view, 4> methodology{
        "methodologies_tbl", "nature_dimensions_tbl", "origin_dimensions_tbl",
        "treatment_dimensions_tbl"};
    const auto ends = [&](const auto& set) {
        return std::ranges::any_of(set, [&](auto s) {
            return table.ends_with(s);
        });
    };
    if (table.ends_with("_artefact_tbl"))
        return {"dq_staging", "Data Staging", "#F3E5F5", 4.1};
    if (ends(datasets))
        return {"dq_datasets", "Dataset Management", "#E3F2FD", 4.2};
    if (ends(methodology))
        return {"dq_methodology", "Methodology", "#E8F5E9", 4.3};
    return {"dq_metadata", "Data Catalog", "#E1F5FE", 4.4};
}

/**
 * @brief str.title() of an ASCII name: each run of letters starts upper
 * case and goes on lower case.
 */
std::string title(std::string_view s) {
    std::string out(s);
    bool previous_letter = false;
    for (auto& c : out) {
        const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (letter && !previous_letter && c >= 'a')
            c = static_cast<char>(c - 'a' + 'A');
        else if (letter && previous_letter && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        previous_letter = letter;
    }
    return out;
}

std::string formatted(const std::string& description) {
    if (description.empty())
        return {};
    const auto lines = split(description, '\n');
    std::string out(lines.front());
    for (std::size_t i = 1; i < lines.size(); ++i)
        out += "\n    " + std::string(lines[i]);
    return out;
}

bool is_temporal(std::string_view name) {
    return name == "valid_from" || name == "valid_to";
}

er_table drawn(const parsed_table& t, const er_conventions& conventions) {
    er_table out;
    out.name = t.name;
    out.color = "#FFFFFF";
    for (const auto& s : conventions.styles) {
        if (s.classification == t.classification) {
            out.stereotype = s.stereotype;
            out.color = s.color;
        }
    }
    out.description = formatted(t.description);
    const bool junction = t.classification == "junction";
    for (const auto& name : t.key) {
        if (junction ? name == "valid_to" : is_temporal(name))
            continue;
        const auto c = std::ranges::find(t.columns, name, &er_column::name);
        out.key.push_back({name, c->type, false});
    }
    for (const auto& c : t.columns) {
        if (!c.is_pk || !c.is_fk)
            continue;
        for (auto& k : out.key) {
            if (k.name == c.name)
                k.is_fk = true;
        }
    }
    std::vector<er_column> shown = t.columns;
    for (auto& c : shown) {
        if (is_temporal(c.name) && !(junction && c.name == "valid_from"))
            c.is_pk = false;
    }
    for (const auto& c : shown) {
        if (!c.is_pk && !is_temporal(c.name))
            out.columns.push_back(c);
    }
    for (const auto& c : shown) {
        if (is_temporal(c.name) && !c.is_pk)
            out.columns.push_back(c);
    }
    return out;
}

}

er_model er_model_of(std::span<const er_script> scripts,
    const er_conventions& conventions) {
    parser p{conventions, {}, {}, {}, {}, {}};
    for (const auto& s : scripts)
        p.parse(s);
    p.mark_columns();
    er_model out;
    out.relationships = p.relationships();
    for (const auto& t : p.tables) {
        er_defined_table d{t.name, t.path, t.line, {}};
        for (const auto& c : t.columns)
            d.columns.push_back(c.name);
        out.tables.push_back(std::move(d));
    }

    std::vector<std::pair<package_head, std::vector<er_table>>> packages;
    const auto package = [&](const package_head& head) -> auto& {
        for (auto& [h, tables] : packages) {
            if (h.name == head.name)
                return tables;
        }
        packages.emplace_back(head, std::vector<er_table>{});
        return packages.back().second;
    };
    for (const auto& t : p.tables) {
        if (t.component == "dq") {
            package(dq_package_of(t.name)).push_back(drawn(t, conventions));
            continue;
        }
        package_head head{t.component, title(t.component), "#FFFFFF", 99};
        for (const auto& c : conventions.components) {
            if (c.name == t.component) {
                head = {c.name, c.description, c.color, c.order};
                break;
            }
        }
        package(head).push_back(drawn(t, conventions));
    }
    std::ranges::stable_sort(packages, {}, [](const auto& p) {
        return p.first.order;
    });
    for (auto& [head, tables] : packages) {
        std::ranges::sort(tables, {}, &er_table::name);
        out.packages.push_back({head.name, head.description, head.color,
            std::move(tables)});
    }
    return out;
}

}
