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
#include "compass.crosscheck/ops/curve_wiring.hpp"

#include <algorithm>
#include <set>
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::ops::curve {

namespace {

namespace pt = compass::platform::text;

constexpr std::string_view root_element = "CurveConfiguration";

/**
 * @brief Where the script stops: a SystemExit, or the IndexError of a
 * brace walk that runs off the text.
 */
struct stop {};

template <typename T>
T* find_key(std::vector<std::pair<std::string, T>>& v, std::string_view k) {
    for (auto& [key, value] : v)
        if (key == k)
            return &value;
    return nullptr;
}

template <typename T>
const T* find_key(const std::vector<std::pair<std::string, T>>& v,
    std::string_view k) {
    for (const auto& [key, value] : v)
        if (key == k)
            return &value;
    return nullptr;
}

/**
 * @brief dict[k] = value: replaced in place, or appended.
 */
template <typename T>
void assign(std::vector<std::pair<std::string, T>>& v, std::string key,
    T value) {
    if (auto* found = find_key(v, key))
        *found = std::move(value);
    else
        v.emplace_back(std::move(key), std::move(value));
}

template <typename T>
T& set_default(std::vector<std::pair<std::string, T>>& v, std::string key,
    T value) {
    if (auto* found = find_key(v, key))
        return *found;
    v.emplace_back(std::move(key), std::move(value));
    return v.back().second;
}

void push_unique(std::vector<std::string>& v, std::string_view s) {
    if (std::ranges::find(v, s) == v.end())
        v.emplace_back(s);
}

/**
 * @brief The text from @p index, just past an opening brace, to its
 * matching closing brace.
 */
std::string_view braced(std::string_view text, std::size_t index) {
    const auto begin = index;
    int depth = 1;
    while (depth > 0) {
        if (index >= text.size())
            throw stop{};
        if (text[index] == '{')
            ++depth;
        else if (text[index] == '}')
            --depth;
        ++index;
    }
    return text.substr(begin, index - 1 - begin);
}

std::optional<std::string_view> function_body(std::string_view text,
    std::string_view name) {
    const pt::regex r("^void " + std::string(name) + R"(\((.*?)\)\s*\{)",
        {.dotall = true, .multiline = true});
    const auto m = r.search(text);
    if (!m)
        return std::nullopt;
    return braced(text, m->end());
}

std::optional<std::string_view> definition_body(std::string_view text,
    std::string_view name) {
    const pt::regex r(R"(\b)" + std::string(name) + R"(\([^;{}]*\)\s*\{)");
    const auto m = r.search(text);
    if (!m)
        return std::nullopt;
    return braced(text, m->end());
}

std::vector<std::pair<std::string, std::string>> declared_sections(
    std::string_view text) {
    static const pt::regex constants(
        R"re(constexpr std::string_view (\w+)_section =\s*"(\w+)";)re");
    static const pt::regex table_re(
        R"(const std::vector<section_access>& sections\(\)\s*\{(.*?)\n    \};)",
        {.dotall = true});
    static const pt::regex entry_re(R"re(\{(\w+)_section,\s*"(\w+)"\})re");
    std::vector<std::pair<std::string, std::string>> names;
    for (const auto& m : constants.find_all(text))
        assign(names, std::string(m.group(1)), std::string(m.group(2)));
    const auto table = table_re.search(text);
    if (!table)
        throw stop{};
    std::vector<std::pair<std::string, std::string>> sections;
    for (const auto& m : entry_re.find_all(table->group(1))) {
        const auto* element = find_key(names, m.group(1));
        if (element == nullptr)
            throw stop{};
        sections.emplace_back(*element, m.group(2));
    }
    return sections;
}

std::vector<std::pair<std::string, std::string>> entry_imports(
    std::string_view text) {
    static const pt::regex r(R"re(for \(const auto& e : each\("(\w+)", "(\w+)"\)\)\s*\n\s*(import_\w+)\()re");
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& m : r.find_all(text))
        assign(out, std::string(m.group(1)), std::string(m.group(3)));
    return out;
}

struct assignment {
    std::string_view variable;
    std::string_view column;
    std::string_view value;
};

std::vector<assignment> rows_of(std::string_view body) {
    static const pt::regex r(R"((\w+)\.(\w+)\s*=\s*([^;]+);)");
    std::vector<assignment> out;
    for (const auto& m : r.find_all(body))
        out.push_back({m.group(1), m.group(2), m.group(3)});
    return out;
}

std::vector<std::pair<std::string, std::string>> stored_rows(
    std::string_view body) {
    static const pt::regex r(R"(out\.(\w+)\.push_back\(std::move\((\w+)\)\))");
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& m : r.find_all(body))
        out.emplace_back(m.group(1), m.group(2));
    return out;
}

std::vector<std::pair<std::string, std::string>> entities_of(
    std::string_view body) {
    static const pt::regex r(R"(refdata::domain::(\w+)\s+(\w+)\s*;)");
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& m : r.find_all(body))
        assign(out, std::string(m.group(2)), std::string(m.group(1)));
    return out;
}

std::vector<std::string> definition_leaves(std::string_view body) {
    static const pt::regex call(R"(add_definition\(out,\s*\w+_section,(.*?)\);)",
        {.dotall = true});
    static const pt::regex text_re(R"re(required_text\(v,\s*"(\w+)"\))re");
    const auto m = call.search(body);
    std::vector<std::string> out;
    if (!m)
        return out;
    for (const auto& t : text_re.find_all(m->group(1)))
        out.emplace_back(t.group(1));
    return out;
}

/**
 * @brief " ".join(@p s.split()).
 */
std::string flatten(std::string_view s) {
    std::string out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && compass::cpp::ops::is_space(s[i]))
            ++i;
        const auto start = i;
        while (i < s.size() && !compass::cpp::ops::is_space(s[i]))
            ++i;
        if (i > start) {
            if (!out.empty())
                out.push_back(' ');
            out.append(s.substr(start, i - start));
        }
    }
    return out;
}

/**
 * @brief arguments.split(",")[0].strip().lstrip("*").
 */
std::string first_argument(std::string_view arguments) {
    auto first = arguments.substr(0, arguments.find(','));
    while (!first.empty() && compass::cpp::ops::is_space(first.front()))
        first.remove_prefix(1);
    while (!first.empty() && compass::cpp::ops::is_space(first.back()))
        first.remove_suffix(1);
    while (!first.empty() && first.front() == '*')
        first.remove_prefix(1);
    return std::string(first);
}

std::optional<std::string> first_quoted(std::string_view flat) {
    static const pt::regex r(R"re("(\w+)")re");
    const auto m = r.search(flat);
    if (!m)
        return std::nullopt;
    return std::string(m->group(1));
}

/**
 * @brief The reader variable and the element an assignment reads, or
 * nothing.
 */
std::optional<std::pair<std::string, std::string>> read_element(
    std::string_view expression) {
    static const pt::regex call(R"((\w+)\((.*)\))");
    static const pt::regex method(
        R"re((\w+)(?:\.|->)(has|text|spelling|integer|real|flag|decimal)\("(\w+)"\))re");
    const auto flat = flatten(expression);
    if (const auto c = call.fullmatch(flat)) {
        const auto first = first_argument(c->group(2));
        const auto element = first_quoted(flat);
        if (first.starts_with('v') && element)
            return std::pair{first, *element};
        return std::nullopt;
    }
    if (const auto m = method.fullmatch(flat))
        return std::pair{std::string(m->group(1)), std::string(m->group(3))};
    return std::nullopt;
}

std::optional<std::string> kind_leaf_of(std::string_view expression,
    const std::optional<std::string>& reader) {
    static const pt::regex call(R"((\w+)\((.*)\))");
    static const pt::regex method(R"re((\w+)(?:\.|->)\w+\("(\w+)"\))re");
    const auto flat = flatten(expression);
    if (const auto c = call.fullmatch(flat)) {
        const auto first = first_argument(c->group(2));
        const auto element = first_quoted(flat);
        if (reader && first == *reader && element)
            return element;
        return std::nullopt;
    }
    if (const auto m = method.fullmatch(flat); m && reader &&
        m->group(1) == *reader)
        return std::string(m->group(2));
    return std::nullopt;
}

std::vector<std::string> blocks_of(std::string_view body) {
    static const pt::regex r(R"re((\w+)(?:\.|->)child\("(\w+)"\))re");
    std::vector<std::string> names;
    for (const auto& m : r.find_all(body))
        if (m.group(1) == "v")
            push_unique(names, m.group(2));
    return names;
}

std::string declared_entity(std::string_view body) {
    static const pt::regex r(R"(refdata::domain::(\w+) \w+;)");
    const auto m = r.search(body);
    return m ? std::string(m->group(1)) : std::string();
}

std::string block_entity(std::string_view body, std::string_view element,
    std::string_view text) {
    const pt::regex opened_re(R"re(const auto (\w+) = v\.child\(")re" +
        std::string(element) + R"re("\); \1\) \{)re");
    const auto opened = opened_re.search(body);
    if (!opened)
        return {};
    const auto branch = braced(body, opened->end());
    const pt::regex call_re(R"(\b(import_\w+)\(ctx, [\w.]+, \*)" +
        std::string(opened->group(1)) + R"(\);)");
    if (const auto call = call_re.search(branch)) {
        const auto reader = definition_body(text, call->group(1));
        return declared_entity(reader.value_or(std::string_view{}));
    }
    return declared_entity(branch);
}

std::vector<std::pair<std::string, std::pair<std::string, std::string>>>
part_traits(std::string_view text) {
    static const pt::regex r(
        R"(struct document_part<refdata::domain::(\w+)> \{\s*)"
        R"(static constexpr auto vector\s*=\s*&\w+::(\w+);\s*)"
        R"(static constexpr auto key\s*=\s*&refdata::domain::\w+::(\w+);)");
    std::vector<std::pair<std::string, std::pair<std::string, std::string>>>
        out;
    for (const auto& m : r.find_all(text))
        assign(out, std::string(m.group(1)),
            std::pair{std::string(m.group(2)), std::string(m.group(3))});
    return out;
}

using vectors_by_part = std::vector<std::pair<std::string, std::string>>;

vectors_by_part part_vectors(std::string_view text) {
    vectors_by_part out;
    for (auto& [entity, trait] : part_traits(text))
        out.emplace_back(entity, trait.first);
    return out;
}

const pt::regex& part_call() {
    static const pt::regex r(R"(\b(import_\w+)\(ctx, \w+, (\w+)[,)])");
    return r;
}

void part_leaves(std::string_view text, std::string_view name,
    const vectors_by_part& vectors, std::vector<std::string>& problems,
    std::vector<std::string>& parts, std::vector<kind_leaf>& leaves) {
    const pt::regex signature_re("^void " + std::string(name) +
        R"(\(([^)]*)\)\s*\{)", {.multiline = true});
    static const pt::regex reader_re(R"(const record_reader& (\w+))");
    static const pt::regex declared_re(R"(refdata::domain::(\w+) (\w+);)");
    const auto signature = signature_re.search(text);
    std::optional<std::string_view> body;
    std::optional<std::string> reader;
    if (signature) {
        if (const auto r = reader_re.search(signature->group(1)))
            reader = std::string(r->group(1));
        body = definition_body(text, name);
    }
    if (!body) {
        problems.push_back("the projection has no part reader " +
            std::string(name));
        return;
    }
    const auto rows = rows_of(*body);
    for (const auto& d : declared_re.find_all(*body)) {
        const auto entity = std::string(d.group(1));
        const auto* vector = find_key(vectors, entity);
        if (vector == nullptr) {
            problems.push_back("the part " + entity + " that " +
                std::string(name) + " fills has no vector");
            continue;
        }
        parts.push_back(entity);
        for (const auto& row : rows) {
            if (row.variable != d.group(2))
                continue;
            if (auto ore = kind_leaf_of(row.value, reader))
                leaves.push_back({std::string(row.column), std::move(*ore),
                    entity, *vector});
        }
    }
    for (const auto& call : part_call().find_all(*body))
        part_leaves(text, call.group(1), vectors, problems, parts, leaves);
}

std::pair<std::vector<collection>, std::vector<std::string>>
collection_rules(std::string_view text) {
    const auto segment = definition_body(text, "segment");
    const auto quotes = definition_body(text, "import_quotes");
    if (!segment)
        return {{}, {"the projection has no import_context::segment"}};
    if (!quotes)
        return {{}, {"the projection has no import_quotes"}};
    static const pt::regex row_re(R"(refdata::domain::(\w+)\s+(\w+);)");
    static const pt::regex vector_re(R"(out\.(\w+)\.push_back)");
    const auto row = row_re.search(*segment);
    const auto vector = vector_re.search(*segment);
    const auto quote_row = row_re.search(*quotes);
    std::vector<std::string> problems;
    if (!row)
        problems.emplace_back("the projection does not state the segment row");
    if (!vector)
        problems.emplace_back(
            "the projection does not state the segment vector");
    if (!quote_row)
        problems.emplace_back("the projection does not state the quote row");
    if (!problems.empty())
        return {{}, problems};
    const auto parent = pt::regex(R"(\b)" + std::string(row->group(2)) +
        R"(\.(\w+)\s*=\s*definition_id;)").search(*segment);
    const auto kind = pt::regex(R"(\b)" + std::string(row->group(2)) +
        R"(\.(\w+)\s*=\s*type;)").search(*segment);
    const auto quote_parent = pt::regex(R"(\b)" +
        std::string(quote_row->group(2)) +
        R"(\.(\w+)\s*=\s*segment_id;)").search(*quotes);
    std::set<std::string> segment_keys;
    const auto prefix = std::string(row->group(1)) + "_";
    for (const auto& [entity, trait] : part_traits(text))
        if (entity.starts_with(prefix))
            segment_keys.insert(trait.second);
    std::optional<std::string> part_parent;
    if (segment_keys.size() == 1) {
        static const pt::regex word(R"((\w+))");
        if (word.fullmatch(*segment_keys.begin()))
            part_parent = *segment_keys.begin();
    }
    if (!parent)
        problems.emplace_back(
            "the projection does not state the segment's key to its entry");
    if (!part_parent)
        problems.emplace_back(
            "the projection does not state the part's key to its segment");
    if (!kind)
        problems.emplace_back(
            "the projection does not state the segment's kind column");
    if (!quote_parent)
        problems.emplace_back(
            "the projection does not state the quote's key to its segment");
    if (!problems.empty())
        return {{}, problems};
    collection c;
    c.entity = row->group(1);
    c.vector = vector->group(1);
    c.parent_key = parent->group(1);
    c.kind_column = kind->group(1);
    c.quote_parent = quote_parent->group(1);
    c.part_parent = *part_parent;
    return {{std::move(c)}, {}};
}

std::optional<std::string_view> complex_type(std::string_view text,
    std::string_view name) {
    const pt::regex r(R"re(<xs:complexType name=")re" + std::string(name) +
        R"re(">(.*?)\n  </xs:complexType>)re", {.dotall = true});
    const auto m = r.search(text);
    if (!m)
        return std::nullopt;
    return m->group(1);
}

std::optional<std::vector<std::string>> children_of(std::string_view text,
    std::string_view name) {
    const auto body = complex_type(text, name);
    if (!body)
        return std::nullopt;
    static const pt::regex element_re(R"(<xs:element\b([^>]*?)(/?)>|</xs:element>)");
    static const pt::regex name_re(R"re(\bname="(\w+)")re");
    std::vector<std::string> children;
    int depth = 0;
    for (const auto& m : element_re.find_all(*body)) {
        if (m.group(0).starts_with("</")) {
            --depth;
            continue;
        }
        if (depth == 0)
            if (const auto found = name_re.search(m.group(1)))
                children.emplace_back(found->group(1));
        if (m.group(2).empty())
            ++depth;
    }
    return children;
}

std::optional<section> section_wiring(std::string_view text,
    const std::vector<schema_section>& declared, const std::string& name,
    const std::string& entry, std::string_view import_name,
    const std::vector<collection>& segment_rules,
    std::vector<std::string>& problems) {
    const auto body_found = function_body(text, import_name);
    if (!body_found) {
        problems.push_back(name + ": the import function " +
            std::string(import_name) + " was not found");
        return std::nullopt;
    }
    const auto body = *body_found;
    const auto entity_of = entities_of(body);
    std::vector<std::pair<std::string, std::string>> vectors_of;
    for (const auto& [vector, variable] : stored_rows(body)) {
        const auto* entity = find_key(entity_of, variable);
        if (entity == nullptr) {
            problems.push_back(name + ": " + variable +
                " is not a declared row of the function");
            continue;
        }
        assign(vectors_of, *entity, vector);
    }
    const auto leaves = definition_leaves(body);
    if (leaves.size() != 2)
        problems.push_back(
            name + ": the entry's identifier and description were not read");
    section s;
    s.name = name;
    s.entry_element = entry;
    s.parameters.push_back({std::string(definition_entity), "curve_id",
        leaves.empty() ? "CurveId" : leaves[0], {}});
    s.parameters.push_back({std::string(definition_entity), "description",
        leaves.size() > 1 ? leaves[1] : "CurveDescription", {}});
    const auto blocks = blocks_of(body);
    std::vector<std::pair<std::string, std::string>> presence;
    const auto is_block = [&](std::string_view e) {
        return std::ranges::find(blocks, e) != blocks.end();
    };
    for (const auto& row : rows_of(body)) {
        const auto read = read_element(row.value);
        if (!read || read->first != "v")
            continue;
        const auto* entity = find_key(entity_of, row.variable);
        if (entity == nullptr)
            continue;
        if (is_block(read->second) && row.column.starts_with("has_")) {
            assign(presence, read->second, std::string(row.column));
            continue;
        }
        s.parameters.push_back({*entity, std::string(row.column),
            read->second, {}});
    }
    const auto vectors = part_vectors(text);
    static const pt::regex entry_part_call(
        R"(\b(import_\w+)\(ctx, [\w.]+, v\);)");
    std::vector<std::string> called;
    for (const auto& m : entry_part_call.find_all(body))
        push_unique(called, m.group(1));
    for (const auto& c : called) {
        std::vector<std::string> parts;
        std::vector<kind_leaf> read;
        part_leaves(text, c, vectors, problems, parts, read);
        for (const auto& part : parts)
            set_default(vectors_of, part, *find_key(vectors, part));
        for (auto& leaf : read) {
            if (is_block(leaf.ore) && leaf.column.starts_with("has_")) {
                assign(presence, leaf.ore, leaf.column);
                continue;
            }
            s.parameters.push_back({leaf.entity, leaf.column, leaf.ore, {}});
        }
    }
    const schema_section* schema = nullptr;
    for (const auto& d : declared)
        if (d.section == name)
            schema = &d;
    if (schema != nullptr && schema->entry != entry)
        problems.push_back(name + ": the projection writes entries as " +
            entry + ", the schema as " + schema->entry);
    for (auto& p : s.parameters) {
        p.xpath = std::string(root_element) + "/" + name + "/" + entry + "/" +
            p.ore;
        if (schema == nullptr ||
            std::ranges::find(schema->leaves, p.ore) == schema->leaves.end())
            problems.push_back(name + ": the schema declares no " + p.ore +
                " under " + entry);
    }
    static const pt::regex segment_import(
        R"re(const auto (\w+) = v\.child\("(\w+)"\); \1\)\s*\n\s*import_segments\(ctx, \*\1\);)re");
    if (const auto segments = segment_import.search(body)) {
        for (const auto& rule : segment_rules) {
            auto c = rule;
            c.element = segments->group(2);
            s.collections.push_back(std::move(c));
        }
    } else if (is_block("Segments")) {
        problems.push_back(name +
            ": holds a Segments block the collection rule does not read");
    }
    for (const auto& element : blocks) {
        const auto* p = find_key(presence, element);
        s.blocks.push_back({element, p ? *p : std::string(),
            p ? std::string() : block_entity(body, element, text)});
    }
    for (const auto& b : s.blocks) {
        const auto& entity = b.presence_entity;
        if (entity.empty() || find_key(vectors_of, entity) != nullptr)
            continue;
        if (const auto* v = find_key(vectors, entity))
            vectors_of.emplace_back(entity, *v);
    }
    for (auto& [entity, vector] : vectors_of)
        s.entities.push_back({entity, vector});
    return s;
}

}

std::optional<std::vector<schema_section>> leaves_of_schema(
    std::string_view schema) {
    static const pt::regex element_re(R"re(<xs:element type="(\w+)" name="(\w+)")re");
    const auto root = complex_type(schema, "curveconfiguration");
    if (!root)
        return std::nullopt;
    std::vector<schema_section> declared;
    for (const auto& m : element_re.find_all(*root)) {
        const auto entries = complex_type(schema, m.group(1));
        if (!entries)
            return std::nullopt;
        const auto found = element_re.find_all(*entries);
        if (found.size() != 1)
            continue;
        auto leaves = children_of(schema, found[0].group(1));
        if (!leaves)
            return std::nullopt;
        schema_section s{std::string(m.group(2)),
            std::string(found[0].group(2)), std::move(*leaves)};
        bool replaced = false;
        for (auto& d : declared)
            if (d.section == s.section) {
                d = s;
                replaced = true;
            }
        if (!replaced)
            declared.push_back(std::move(s));
    }
    return declared;
}

std::pair<std::vector<segment_kind>, std::vector<std::string>> kind_leaves(
    std::string_view projection) {
    try {
        const auto body = definition_body(projection, "import_segments");
        if (!body)
            return {{}, {"the projection has no import_segments"}};
        const auto vectors = part_vectors(projection);
        if (vectors.empty())
            return {{}, {"the projection names no segment part vectors"}};
        static const pt::regex loop(
            R"re(for \(const auto& (\w+) : v\.children\("(\w+)"\)\) \{(.*?)\n    \})re",
            {.dotall = true});
        std::vector<segment_kind> kinds;
        std::vector<std::string> problems;
        for (const auto& l : loop.find_all(*body)) {
            segment_kind k;
            k.kind = l.group(2);
            for (const auto& call : part_call().find_all(l.group(3)))
                part_leaves(projection, call.group(1), vectors, problems,
                    k.parts, k.leaves);
            if (k.leaves.empty())
                problems.push_back(
                    "the projection reads no leaves for the kind " + k.kind);
            kinds.push_back(std::move(k));
        }
        if (kinds.empty())
            problems.emplace_back("the projection reads no segment kinds");
        return {kinds, problems};
    } catch (const stop&) {
        return {{}, {"stopped"}};
    }
}

std::pair<types_by_part, std::vector<std::string>> default_curve_types(
    std::string_view projection) {
    try {
        const auto body = definition_body(projection,
            "parts_of_default_curve_type");
        if (!body)
            return {{}, {"the projection has no parts_of_default_curve_type"}};
        static const pt::regex branch(
            R"((?:if|else if) \(([^{]*?)\) \{((?:[^{}]|\{[^{}]*\})*)\})",
            {.dotall = true});
        static const pt::regex type_re(R"re(type == "(\w+)")re");
        static const pt::regex part_re(R"(\.(\w+) = true)");
        const auto vectors = part_vectors(projection);
        std::vector<std::string> problems;
        types_by_part by_part;
        for (const auto& b : branch.find_all(*body)) {
            std::vector<std::string> types;
            for (const auto& t : type_re.find_all(b.group(1)))
                types.emplace_back(t.group(1));
            for (const auto& p : part_re.find_all(b.group(2))) {
                const auto part = "default_curve_" + std::string(p.group(1));
                if (find_key(vectors, part) == nullptr) {
                    problems.push_back("the default curve part " + part +
                        " has no vector");
                    continue;
                }
                auto& seen = set_default(by_part, part,
                    std::vector<std::string>{});
                for (const auto& t : types)
                    push_unique(seen, t);
            }
        }
        if (by_part.empty())
            problems.emplace_back(
                "the projection's default curve dispatch fills no parts");
        return {by_part, problems};
    } catch (const stop&) {
        return {{}, {"stopped"}};
    }
}

std::optional<wiring_result> wiring(std::string_view projection,
    const std::vector<schema_section>& declared) {
    try {
        const auto imports = entry_imports(projection);
        wiring_result r;
        auto [segment_rules, rule_problems] = collection_rules(projection);
        r.problems = rule_problems;
        auto [kinds, kind_problems] = kind_leaves(projection);
        r.problems.insert(r.problems.end(), kind_problems.begin(),
            kind_problems.end());
        for (auto& rule : segment_rules)
            rule.kinds = kinds;
        const auto sections = declared_sections(projection);
        for (const auto& [name, entry] : sections) {
            const auto* import_name = find_key(imports, name);
            if (import_name == nullptr) {
                r.problems.push_back(
                    name + ": the document mapper does not import the section");
                continue;
            }
            if (auto s = section_wiring(projection, declared, name, entry,
                    *import_name, segment_rules, r.problems))
                r.sections.push_back(std::move(*s));
        }
        bool same = declared.size() == sections.size();
        for (std::size_t i = 0; same && i < sections.size(); ++i)
            same = declared[i].section == sections[i].first;
        if (!same)
            r.problems.emplace_back("the projection's sections and the "
                "schema's are not the same list, in the same order");
        return r;
    } catch (const stop&) {
        return std::nullopt;
    }
}

std::string fill(std::string_view text, std::size_t width,
    std::string_view indent) {
    std::vector<std::string_view> words;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && text[i] == ' ')
            ++i;
        const auto start = i;
        while (i < text.size() && text[i] != ' ')
            ++i;
        if (i > start)
            words.push_back(text.substr(start, i - start));
    }
    std::string out;
    const auto room = width - indent.size();
    std::string line;
    const auto flush = [&] {
        if (!out.empty())
            out.push_back('\n');
        out.append(indent);
        out.append(line);
        line.clear();
    };
    for (auto word : words) {
        if (!line.empty() && line.size() + 1 + word.size() > room)
            flush();
        while (line.empty() && word.size() > room) {
            line = word.substr(0, room);
            word.remove_prefix(room);
            flush();
        }
        if (word.empty())
            continue;
        if (!line.empty())
            line.push_back(' ');
        line.append(word);
    }
    if (!line.empty())
        flush();
    return out;
}

}
