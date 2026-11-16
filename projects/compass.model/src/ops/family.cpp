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
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

/**
 * @brief One row of * Parts: a member name, its entity, its cardinality
 * and, for one_of, its group.
 */
struct part {
    std::string_view name;
    std::string_view entity;
    std::string_view cardinality;
    std::string_view group;
};

std::vector<part> parts_of(const outline& o, node_id document) {
    const auto section_node = section(o, document, "Parts");
    std::vector<part> out;
    if (section_node.is_none())
        return out;
    for (const auto& row : first_table_rows(o, section_node)) {
        const part p{trim(row.value_or("part")), trim(row.value_or("entity")),
            trim(row.value_or("cardinality")), trim(row.value_or("group"))};
        if (p.name.empty() || p.entity.empty())
            throw model_error("every row of '* Parts' must name a part and an "
                "entity");
        if (std::ranges::find(out, p.name, &part::name) != out.end())
            throw model_error("the part '" + std::string(p.name) +
                "' is listed more than once");
        if (p.cardinality != "one" && p.cardinality != "optional" &&
            p.cardinality != "one_of")
            throw model_error("the part '" + std::string(p.name) +
                "' must state its cardinality as one, optional or one_of");
        if ((p.cardinality == "one_of") != !p.group.empty())
            throw model_error("the part '" + std::string(p.name) +
                "' must name a group exactly when its cardinality is one_of");
        out.push_back(p);
    }
    return out;
}

/**
 * @brief _part_cardinality_view: the parts each element fills once, the
 * ones it fills at most once, and the groups of alternatives, in the
 * order each group is first named.
 */
void write_cardinality_view(builder& b, value_id family,
    const std::vector<part>& parts) {
    std::vector<std::pair<std::string_view, std::vector<std::string_view>>>
        groups;
    for (const auto& p : parts) {
        if (p.cardinality != "one_of")
            continue;
        auto it = std::ranges::find(groups, p.group,
            &decltype(groups)::value_type::first);
        if (it == groups.end())
            it = groups.insert(groups.end(), {p.group, {}});
        it->second.push_back(p.name);
    }
    for (const auto& [name, members] : groups) {
        if (members.size() < 2)
            throw model_error("the group '" + std::string(name) +
                "' names one part; a group needs two or more");
    }
    for (const auto cardinality : {"one", "optional"}) {
        const auto list = b.list(family,
            "cardinality_" + std::string(cardinality));
        for (const auto& p : parts) {
            if (p.cardinality == cardinality)
                b.set(b.append_object(list), "part", p.name);
        }
    }
    const auto list = b.list(family, "cardinality_groups");
    for (const auto& [name, members] : groups) {
        const auto g = b.append_object(list);
        std::string label;
        for (const auto m : members)
            label += (label.empty() ? "" : " or ") + std::string(m);
        b.set(g, "label", label);
        const auto terms = b.list(g, "terms");
        for (std::size_t i = 0; i < members.size(); ++i) {
            const auto t = b.append_object(terms);
            b.set(t, "part", members[i]);
            b.set(t, "last", i + 1 == members.size());
        }
    }
}

/*
 * ^#\+<key>:\s*(\S.*?)\s*$ over the first 4096 bytes, ignoring case and
 * multi-line, as _frontmatter_scalar reads it.
 */
std::string_view frontmatter_scalar(std::string_view text,
    std::string_view key) {
    text = text.substr(0, 4096);
    const auto head = "#+" + std::string(key) + ":";
    for (std::size_t start = 0; start < text.size();) {
        auto end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        const auto line_start = start;
        start = end + 1;
        if (!iequals(text.substr(line_start, head.size()), head))
            continue;
        auto p = line_start + head.size();
        while (p < text.size() && is_space(text[p]))
            ++p;
        if (p == text.size())
            continue;
        auto stop = text.find('\n', p);
        if (stop == std::string_view::npos)
            stop = text.size();
        return trim(text.substr(p, stop - p));
    }
    return {};
}

/*
 * _entity_model_paths: the ores.codegen.entity models directly in the
 * directory of @p path, by the singular each declares, the first in path
 * order winning.
 */
std::map<std::string_view, std::string_view, std::less<>> entity_model_paths(
    const load_context& c, std::string_view path) {
    const auto dir = path.substr(0, path.rfind('/') + 1);
    auto paths = c.list(dir);
    std::erase_if(paths, [&](std::string_view p) {
        const auto name = p.substr(dir.size());
        return name.contains('/') || name.starts_with('.') ||
            !name.ends_with(".org");
    });
    std::ranges::sort(paths);
    std::map<std::string_view, std::string_view, std::less<>> out;
    for (const auto p : paths) {
        const auto d = c.find(p);
        if (!d || frontmatter_scalar(d->source, "type") !=
            "ores.codegen.entity")
            continue;
        const auto singular = frontmatter_scalar(d->source, "entity_singular");
        if (!singular.empty())
            out.try_emplace(singular, p);
    }
    return out;
}

std::vector<value_id> items(const builder& b, value_id list) {
    std::vector<value_id> out;
    if (list.is_none())
        return out;
    const auto& v = b.tables().values;
    for (auto i = v.firsts[list]; !i.is_none(); i = v.nexts[i])
        out.push_back(i);
    return out;
}

std::string member_text(const builder& b, value_id object,
    std::string_view key) {
    return object.is_none() ? std::string() : text_value(b, b.find(object, key));
}

/*
 * _column_name: column, field or name, the first that is set.
 */
std::string column_name(const builder& b, value_id column) {
    for (const auto key : {"column", "field", "name"}) {
        if (truthy(b, b.find(column, key)))
            return member_text(b, column, key);
    }
    return {};
}

/*
 * _key_cpp_type: the column's cpp_type, else a uuid when it is_uuid, else
 * a string.
 */
std::string key_cpp_type(const builder& b, value_id column) {
    if (truthy(b, b.find(column, "cpp_type")))
        return member_text(b, column, "cpp_type");
    return truthy(b, b.find(column, "is_uuid")) ? "boost::uuids::uuid"
        : "std::string";
}

struct field {
    std::string name;
    std::string cpp_type;
};

/*
 * key_record_fields: the declared key's column when the model declares
 * one (key_field, else presentation.key_field), else the primary key's
 * columns.
 */
std::vector<field> key_record_fields(const builder& b, value_id entity) {
    std::string declared;
    if (truthy(b, b.find(entity, "key_field")))
        declared = member_text(b, entity, "key_field");
    else if (const auto p = b.find(entity, "presentation");
             !p.is_none() && truthy(b, b.find(p, "key_field")))
        declared = member_text(b, p, "key_field");
    const auto primary = b.find(entity, "primary_key");
    const auto pk_columns = primary.is_none() ? std::vector<value_id>()
        : items(b, b.find(primary, "columns"));
    if (!declared.empty()) {
        auto columns = pk_columns;
        for (const auto key : {"natural_keys", "columns"})
            std::ranges::copy(items(b, b.find(entity, key)),
                std::back_inserter(columns));
        for (const auto column : columns) {
            if (column_name(b, column) == declared)
                return {{declared, key_cpp_type(b, column)}};
        }
    }
    std::vector<field> out;
    for (const auto column : pk_columns)
        out.push_back({column_name(b, column), key_cpp_type(b, column)});
    return out;
}

struct message {
    std::string name;
    std::vector<field> fields;
    std::string response_type;
    std::string subject;
};

void write_field_list(builder& b, value_id parent, std::string_view key,
    const std::vector<field>& fields, const custom_types& custom) {
    const auto list = b.list(parent, key);
    for (const auto& f : fields) {
        const auto o = b.append_object(list);
        b.set(o, "name", f.name);
        b.set(o, "cpp_type", f.cpp_type);
        b.set(o, "comment", "");
        if (const auto ts = ts_type(f.cpp_type, custom); !ts.empty())
            b.set(o, "ts_type", ts);
    }
}

/*
 * family_protocol_messages and the TypeScript imports of its messages:
 * the key, item and write records and the get, get_many, put and put_many
 * pairs, with the includes and imports of every entity the family names,
 * the central row first.
 */
void write_protocol(load_context& c, value_id family, value_id central,
    const std::vector<part>& parts) {
    auto& b = c.b;
    const auto& o = c.o;
    const auto& custom = custom_type_headers(c.find, c.cache);
    const std::string component(frontmatter_or(o, "component"));
    const std::string singular(frontmatter_or(o, "entity_singular"));
    const auto plural_of = frontmatter(o, "entity_plural");
    const auto plural = plural_of.found ? std::string(plural_of.value)
        : singular + "s";
    const std::string central_name(member_text(b, family, "central_entity"));
    const auto domain = "ores::" + component + "::domain::";
    const auto key = singular + "_key";
    const auto item = singular + "_item";
    const auto write = singular + "_write";

    std::vector<field> item_fields{{"key", key},
        {central_name, "std::optional<" + domain + central_name + ">"}};
    std::vector<field> write_fields{{"change", central_name + "_change"}};
    std::vector<std::string> entities{central_name};
    for (const auto& p : parts) {
        const std::string entity(p.entity);
        item_fields.push_back({std::string(p.name),
            "std::vector<" + domain + entity + ">"});
        write_fields.push_back({std::string(p.name),
            "std::vector<" + entity + "_change>"});
        if (std::ranges::find(entities, entity) == entities.end())
            entities.push_back(entity);
    }

    constexpr std::string_view result = "ores::utility::domain::result";
    constexpr std::string_view intent = "ores::utility::domain::change_intent";
    const auto subject = [&](std::string_view verb) {
        return component + ".v1." + plural + "." + std::string(verb);
    };
    std::vector<message> messages{{key, key_record_fields(b, central), {}, {}},
        {item, item_fields, {}, {}}, {write, write_fields, {}, {}}};
    const auto pair = [&](const std::string& name, std::string_view verb,
        std::vector<field> request, field response) {
        messages.push_back({name + "_request", std::move(request),
            name + "_response", subject(verb)});
        messages.push_back({name + "_response",
            {{"result", std::string(result)}, std::move(response)}, {}, {}});
    };
    pair("get_" + singular, "get", {{"key", key}},
        {"item", "std::optional<" + item + ">"});
    pair("get_many_" + plural, "get_many", {{"keys", "std::vector<" + key + ">"}},
        {"items", "std::vector<" + item + ">"});
    pair("put_" + singular, "put", {{"item", write},
        {"intent", std::string(intent)}},
        {"item", "std::optional<" + item + ">"});
    pair("put_many_" + plural, "put_many",
        {{"items", "std::vector<" + write + ">"},
         {"intent", std::string(intent)}},
        {"items", "std::vector<" + item + ">"});

    write_field_list(b, family, "key_fields", messages[0].fields, custom);
    write_field_list(b, family, "item_fields", item_fields, custom);
    write_field_list(b, family, "write_fields", write_fields, custom);
    const auto list = b.list(family, "messages");
    std::vector<std::string_view> cpp_types;
    for (const auto& m : messages) {
        const auto e = b.append_object(list);
        b.set(e, "name", m.name);
        b.set(e, "name_pascal", pascal_case(m.name));
        b.set(e, "comment", "");
        write_field_list(b, e, "fields", m.fields, custom);
        b.set(e, "derived", true);
        if (!m.response_type.empty())
            b.set(e, "response_type", m.response_type);
        if (!m.subject.empty()) {
            b.set(e, "subject", m.subject);
            b.set(e, "requires_session", "true");
        }
        for (const auto& f : m.fields)
            cpp_types.push_back(f.cpp_type);
    }

    const auto sub = frontmatter_or(o, "subcomponent");
    const auto include = sub.empty() ? component
        : component + "." + std::string(sub);
    const auto domain_includes = b.list(family, "domain_includes");
    const auto protocol_includes = b.list(family, "protocol_includes");
    const auto protocol_imports = b.list(family, "protocol_imports");
    for (const auto& e : entities) {
        b.append(domain_includes, "ores." + include + "/domain/" + e + ".hpp");
        b.append(protocol_includes,
            "ores." + include + "/messaging/" + e + "_protocol.hpp");
        const auto i = b.append_object(protocol_imports);
        b.set(i, "entity", e);
        b.set(i, "entity_pascal", pascal_case(e));
    }
    const auto domain_imports = b.list(family, "domain_imports");
    for (const auto& i : ts_domain_imports(cpp_types, component, custom)) {
        const auto e = b.append_object(domain_imports);
        b.set(e, "entity", i.entity);
        b.set(e, "entity_pascal", i.entity_pascal);
        b.set(e, "module_dir", i.module_dir);
    }
    const auto utility_imports = b.list(family, "utility_imports");
    for (const auto& i : ts_utility_imports(cpp_types)) {
        const auto e = b.append_object(utility_imports);
        b.set(e, "name_pascal", i.pascal);
        b.set(e, "module", i.module);
    }
}

/*
 * The raw domain_entity of the entity model at @p path, as
 * org_document_to_model reads it, loaded into a detached object.
 */
value_id load_central(load_context& c, std::string_view path) {
    const auto d = c.find(path);
    if (!d)
        throw model_error("no document at " + std::string(path));
    outline o;
    build_outline(d->source, *d->tables, o);
    load_context ec{c.b, *d, o, c.find, c.list, c.cache};
    return entity_document_model(ec, c.b.detached_object());
}

}

void load_family(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    const node_id document{0};
    const auto family = b.object(root, "family");
    for (const auto key : {"product", "component", "subcomponent",
        "entity_singular", "entity_plural", "entity_title", "brief"}) {
        if (const auto r = frontmatter(o, key); r.found)
            b.set(family, key, r.value);
    }
    if (const auto text = body(o, document); !text.empty())
        b.set(family, "description", text);
    if (trim(frontmatter_or(o, "component")).empty())
        throw model_error("a family model must declare #+component:");
    const auto central = trim(property(o, section(o, document, "Flags"),
        "central_entity").value);
    if (central.empty())
        throw model_error("a family model must name its central row with "
            "':central_entity:' in '* Flags'");
    b.set(family, "central_entity", central);
    const auto parts = parts_of(o, document);
    const auto list = b.list(family, "parts");
    for (const auto& p : parts) {
        const auto row = b.append_object(list);
        b.set(row, "part", p.name);
        b.set(row, "entity", p.entity);
        b.set(row, "cardinality", p.cardinality);
        b.set(row, "group", p.group);
    }
    write_cardinality_view(b, family, parts);

    const auto entities = entity_model_paths(c, c.d.path);
    const auto central_path = entities.find(central);
    if (central_path == entities.end())
        throw model_error("the family names the entity '" +
            std::string(central) + "', which no entity model beside it "
            "declares");
    for (const auto& p : parts) {
        if (!entities.contains(p.entity))
            throw model_error("the family names the entity '" +
                std::string(p.entity) + "', which no entity model beside it "
                "declares");
    }
    write_protocol(c, family, load_central(c, central_path->second), parts);
}

}
