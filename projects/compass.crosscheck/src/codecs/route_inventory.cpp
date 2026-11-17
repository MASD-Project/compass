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
#include "compass.crosscheck/codecs/route_inventory.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/python_literals.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;
using ops::python_value;

constexpr std::string_view inventory_path =
    "doc/knowledge/architecture/http_route_inventory.org";
constexpr std::string_view script_path = "build/scripts/route_inventory.py";
constexpr std::string_view classification_path =
    "build/scripts/route_classification.py";
constexpr std::string_view pairing_path = "build/scripts/route_pairing.py";

/**
 * @brief Where the script stops without writing.
 */
struct stop {};

struct route {
    std::string method;
    std::string path;
    std::string server;
    std::string owner;
    std::string where;
    std::string note;
};

struct decision {
    std::string name;
    std::string kind;
    std::string area;
    std::string note;
};

struct rule {
    decision d;
    std::string shape;
    std::string pattern;
    std::optional<std::string> server;
    std::shared_ptr<pt::regex> compiled;
};

using key3 = std::tuple<std::string, std::string, std::string>;
using key2 = std::pair<std::string, std::string>;

struct family {
    std::string spec;
    std::string method;
    std::string tpl;
    std::string needs;
};

struct tables {
    std::string header;
    std::string composed;
    std::string no_area;
    std::vector<rule> rules;
    std::map<key3, decision> exceptions;
    std::map<std::string, std::vector<std::string>, std::less<>> views;
    std::map<key2, key2> pairs;
    std::map<key2, family> families;
};

std::string read(const ct::corpus_view& v, std::string_view path) {
    auto text = v.read_file(path);
    if (!text)
        throw stop{};
    return std::move(*text);
}

python_value module_value(std::string_view source, std::string_view name,
    const std::map<std::string, std::string, std::less<>>& constants) {
    const auto raw = ops::python_assignment(source, name);
    if (!raw)
        throw stop{};
    auto v = ops::resolved(*raw, constants);
    if (!v)
        throw stop{};
    return std::move(*v);
}

const std::string& text_of(const python_value& v) {
    if (v.what != python_value::kind::string)
        throw stop{};
    return v.text;
}

/**
 * @brief The argument of a dataclass call at @p index, or its keyword.
 */
const python_value& argument(const python_value& call, std::size_t index,
    std::string_view keyword) {
    if (index < call.items.size())
        return call.items[index];
    for (const auto& e : call.entries)
        if (e.key == keyword)
            return e.value;
    throw stop{};
}

std::vector<std::string> strings(const python_value& v) {
    auto s = ops::strings_of(v);
    if (!s)
        throw stop{};
    return *s;
}

std::vector<std::string> key_strings(const ops::python_entry& e) {
    return strings({python_value::kind::sequence, {}, e.key_items, {}});
}

tables read_tables(const ct::corpus_view& v) {
    tables t;
    const auto script = read(v, script_path);
    const auto header = ops::python_assignment(script, "HEADER");
    if (!header || header->what != python_value::kind::string)
        throw stop{};
    t.header = header->text;

    const auto source = read(v, classification_path);
    const auto constants = ops::python_string_constants(source);
    t.composed = text_of(module_value(source, "COMPOSED", constants));
    t.no_area = text_of(module_value(source, "NO_AREA", constants));
    for (const auto& r : module_value(source, "RULES", constants).items) {
        rule x;
        x.d.name = text_of(argument(r, 0, "name"));
        const auto match = strings(argument(r, 1, "match"));
        if (match.size() != 2)
            throw stop{};
        x.shape = match[0];
        x.pattern = match[1];
        const auto& server = argument(r, 2, "server");
        if (server.what == python_value::kind::string)
            x.server = server.text;
        x.d.kind = text_of(argument(r, 3, "kind"));
        x.d.area = text_of(argument(r, 4, "area"));
        x.d.note = text_of(argument(r, 5, "note"));
        if (x.shape != "exact" && x.shape != "prefix")
            x.compiled = std::make_shared<pt::regex>(x.pattern);
        t.rules.push_back(std::move(x));
    }
    for (const auto& e : module_value(source, "EXCEPTIONS", constants).entries) {
        const auto k = key_strings(e);
        if (k.size() != 3)
            throw stop{};
        t.exceptions.insert_or_assign({k[0], k[1], k[2]},
            decision{text_of(argument(e.value, 0, "name")),
                text_of(argument(e.value, 1, "kind")),
                text_of(argument(e.value, 2, "area")),
                text_of(argument(e.value, 3, "note"))});
    }
    for (const auto& e : module_value(source, "VIEWS", constants).entries)
        t.views.insert_or_assign(e.key, strings(e.value));

    const auto pairing = read(v, pairing_path);
    const auto pairing_constants = ops::python_string_constants(pairing);
    for (const auto& e :
        module_value(pairing, "PAIRS", pairing_constants).entries) {
        const auto k = key_strings(e);
        const auto target = strings(e.value);
        if (k.size() != 2 || target.size() != 2)
            throw stop{};
        t.pairs.insert_or_assign({k[0], k[1]}, key2{target[0], target[1]});
    }
    for (const auto& e :
        module_value(pairing, "FAMILIES", pairing_constants).entries) {
        const auto k = key_strings(e);
        const auto f = strings(e.value);
        if (k.size() != 2 || f.size() != 4)
            throw stop{};
        t.families.insert_or_assign({k[0], k[1]}, family{f[0], f[1], f[2],
            f[3]});
    }
    return t;
}

std::string upper(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    return out;
}

std::string at(std::string_view path, std::string_view text,
    std::size_t offset) {
    return std::string(path) + ":" + std::to_string(co::line_of(text, offset));
}

std::string stem(std::string_view path) {
    auto name = path.substr(path.rfind('/') + 1);
    const auto dot = name.rfind('.');
    if (dot != std::string_view::npos && dot > 0)
        name = name.substr(0, dot);
    return std::string(name);
}

std::vector<std::string_view> bff_sources(const ct::corpus_view& v) {
    auto paths = co::glob(v, "projects/ores.web/packages/bff/src/**/*.ts");
    std::erase_if(paths, [](std::string_view p) {
        return p.ends_with(".test.ts");
    });
    return paths;
}

std::vector<std::string_view> cpp_sources(const ct::corpus_view& v) {
    auto paths = co::glob(v, "projects/ores.http/*/src/**/*.cpp");
    std::erase_if(paths, [](std::string_view p) {
        return p.find("/tests/") != std::string_view::npos;
    });
    return paths;
}

std::string cpp_method(std::string_view m) {
    return m == "delete_" ? "delete" : std::string(m);
}

std::vector<route> routes(const ct::corpus_view& v) {
    static const pt::regex bff_route(
        R"re(\.(get|post|put|patch|delete)\(\s*(['"`])(/api/[^'"`]*)\2)re");
    static const pt::regex cpp_route(
        R"re(router->(get|post|put|patch|delete_|head|options)\(\s*"(/[^"]*)")re");
    static const pt::regex cpp_variable(
        R"(router->(get|post|put|patch|delete_|head|options)\(\s*([A-Za-z_][\w:]*)\s*[,)])");
    std::vector<route> found;
    for (const auto path : bff_sources(v)) {
        const auto text = read(v, path);
        for (const auto& m : bff_route.find_all(text))
            found.push_back({std::string(m.group(1)), std::string(m.group(3)),
                "bff", stem(path), at(path, text, m.start()), {}});
    }
    constexpr std::string_view http = "projects/ores.http/";
    for (const auto path : cpp_sources(v)) {
        const auto text = read(v, path);
        const auto rest = path.substr(http.size());
        const auto owner = std::string(rest.substr(0, rest.find('/')));
        for (const auto& m : cpp_route.find_all(text))
            found.push_back({cpp_method(m.group(1)), std::string(m.group(2)),
                "ores.http", owner, at(path, text, m.start()), {}});
        for (const auto& m : cpp_variable.find_all(text)) {
            const auto name = std::string(m.group(2));
            const pt::regex declaration(
                R"(^\s*(?:const\s+)?(?:auto|std::string|std::string_view)\s+)" +
                pt::escape(name) + R"(\s*=)");
            std::string declared;
            std::size_t number = 0;
            for (const auto line : co::split_lines(text)) {
                ++number;
                if (declaration.search(line)) {
                    declared = " (declared at " + std::to_string(number) + ")";
                    break;
                }
            }
            found.push_back({cpp_method(m.group(1)), "{" + name + "}",
                "ores.http", owner, at(path, text, m.start()),
                "path composed from " + name + declared + "."});
        }
    }
    std::ranges::stable_sort(found, [](const route& a, const route& b) {
        return std::tie(a.path, a.method, a.server) <
            std::tie(b.path, b.method, b.server);
    });
    return found;
}

const decision* decide(const tables& t, const route& r) {
    const auto method = upper(r.method);
    if (const auto e = t.exceptions.find({r.server, method, r.path});
        e != t.exceptions.end())
        return &e->second;
    const auto target = method + " " + r.path;
    for (const auto& x : t.rules) {
        if (x.server && *x.server != r.server)
            continue;
        if (x.shape == "exact" ? target == x.pattern
            : x.shape == "prefix" ? target.starts_with(x.pattern)
            : x.compiled->search(target).has_value())
            return &x.d;
    }
    return nullptr;
}

std::string joined(const std::vector<std::string>& parts,
    std::string_view separator) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0)
            out += separator;
        out += parts[i];
    }
    return out;
}

/**
 * @brief Areas in the script's order: (none) last, the rest by name.
 */
bool area_before(std::string_view a, std::string_view b,
    std::string_view none) {
    return std::pair(a == none, a) < std::pair(b == none, b);
}

void area_map(const tables& t, const std::vector<route>& found,
    std::vector<std::string>& lines) {
    struct entry {
        std::set<std::string> owners;
        int routes = 0;
        std::map<std::string, int> kinds;
    };
    std::map<std::string, entry> areas;
    for (const auto& r : found) {
        const auto* d = decide(t, r);
        auto& e = areas[d ? d->area : std::string()];
        e.owners.insert(r.owner);
        ++e.routes;
        ++e.kinds[d ? d->kind : "unclassified"];
    }
    std::vector<std::string> order;
    for (const auto& [area, e] : areas)
        order.push_back(area);
    std::ranges::stable_sort(order, [&](const auto& a, const auto& b) {
        return area_before(a, b, t.no_area);
    });
    lines.insert(lines.end(), {"* The area map", "",
        "The published areas (design section 11.2) and the areas this table",
        "publishes beside them, each with the components that answer it today,",
        "its size, and the kinds its routes are decided as. A route that is not",
        "an API publishes no area, so it is counted under =(none)=.", "",
        "| Area | Owner components | Routes | Kinds |",
        "|------+------------------+--------+-------|"});
    for (const auto& area : order) {
        const auto& e = areas.at(area);
        std::vector<std::string> kinds;
        for (const auto& [kind, count] : e.kinds)
            kinds.push_back(kind + " " + std::to_string(count));
        lines.push_back("| " + area + " | " +
            joined({e.owners.begin(), e.owners.end()}, ", ") + " | " +
            std::to_string(e.routes) + " | " + joined(kinds, ", ") + " |");
    }
    lines.emplace_back();
}

/**
 * @brief The client modules a composed view's caller is looked up in, in
 * pathlib's order, each with its text, read once.
 */
using clients = std::vector<std::pair<std::string_view, std::string>>;

clients client_modules(const ct::corpus_view& v) {
    clients out;
    for (const auto path : co::glob(v, "projects/ores.web/packages/**/*.ts")) {
        if (path.find("/bff/") != std::string_view::npos ||
            path.ends_with(".test.ts") || path.ends_with(".d.ts"))
            continue;
        out.emplace_back(path, read(v, path));
    }
    return out;
}

std::string called_by(const clients& modules, std::string_view route_path) {
    const auto literal = route_path.substr(0, route_path.find(':'));
    if (co::characters(literal) < 6)
        return {};
    constexpr std::string_view root = "projects/ores.web/packages/";
    for (const auto& [path, text] : modules)
        if (text.find(literal) != std::string::npos)
            return std::string(path.substr(root.size()));
    return {};
}

void composed_views(const ct::corpus_view& v, const tables& t,
    const std::vector<route>& found, std::vector<std::string>& lines) {
    std::map<std::string, std::vector<std::string>> views;
    for (const auto& r : found) {
        const auto* d = decide(t, r);
        if (d == nullptr || d->kind != t.composed)
            continue;
        auto& addresses = views[d->name];
        const auto address = upper(r.method) + " " + r.path;
        if (std::ranges::find(addresses, address) == addresses.end())
            addresses.push_back(address);
    }
    if (views.empty())
        return;
    lines.insert(lines.end(), {"* The composed views", "",
        "A composed view owes four things, and the fourth is why the owner column",
        "exists: without it the view is the old BFF again. The name is the view's",
        "own; the schema is K1's to generate; the owner is the component that must",
        "answer it after the port, which is not always the one that answers it",
        "today.", "",
        "Every route here is one a screen calls now, and the =Called by= column says",
        "which client module calls it, so the port can prove a view with the same",
        "request against either server. A view keeps its current route until the",
        "cutover for that reason: moving it to the published namespace is a versioned",
        "change of its own, not part of the port.", "",
        "| View | Routes today | Called by | Owner | Computation | Cache policy |",
        "|------+--------------+-----------+-------+-------------+--------------|"});
    const auto modules = client_modules(v);
    for (const auto& [name, addresses] : views) {
        std::vector<std::string> obligations(3);
        if (const auto o = t.views.find(name); o != t.views.end()) {
            if (o->second.size() != 3)
                throw stop{};
            obligations = o->second;
        }
        auto sorted = addresses;
        std::ranges::sort(sorted);
        const auto& first = addresses.front();
        auto caller = called_by(modules, first.substr(first.find(' ') + 1));
        if (caller.empty())
            caller = "no client calls it";
        lines.push_back("| " + name + " | " + joined(sorted, ", ") + " | " +
            caller + " | " + obligations[0] + " | " + obligations[1] + " | " +
            obligations[2] + " |");
    }
    lines.emplace_back();
}

/**
 * @brief members_of(): an entry's own members, one per line from its
 * first column.
 */
std::string members_of(std::string_view body, std::size_t indent) {
    std::vector<std::string> out;
    const std::string pad(indent, ' ');
    for (const auto line : co::split_lines(body))
        if (line.starts_with(pad) && line.substr(indent, 1) != " ")
            out.emplace_back(line.substr(indent));
    return joined(out, "\n");
}

struct resource {
    std::string key;
    bool one_key = false;
    bool list_by = false;
    bool writable = false;
};

std::optional<resource> resource_of(const std::string& members) {
    static const pt::regex key_re(R"(^key: '([^']+)')", {.multiline = true});
    static const pt::regex fields_re(R"(^keyFields: \[([^\]]*)\])",
        {.multiline = true});
    static const pt::regex writable_re(R"(^writable: (true|false))",
        {.multiline = true});
    static const pt::regex quoted(R"('[^']+')");
    const auto key = key_re.search(members);
    if (!key)
        return std::nullopt;
    const auto named = [&](std::string_view member) {
        return pt::regex("^" + std::string(member) + ":", {.multiline = true})
            .search(members).has_value();
    };
    const auto fields = fields_re.search(members);
    const auto writable = writable_re.search(members);
    resource r;
    r.key = key->group(1);
    r.one_key = named("keyField") ||
        (fields && quoted.find_all(fields->group(1)).size() == 1);
    r.list_by = named("listBy") || named("parentField");
    r.writable = writable ? writable->group(1) == "true" : named("writes");
    return r;
}

std::vector<resource> registry_resources(const ct::corpus_view& v,
    std::string_view spec) {
    static const pt::regex array_re(R"(^(?:export )?const (\w+)\b[^=]*= \[\n(.*?)^\];)",
        {.dotall = true, .multiline = true});
    static const pt::regex entry_re(R"(^    \{\n(.*?)^    \},?$)",
        {.dotall = true, .multiline = true});
    static const pt::regex spread_re(
        R"(^    \.\.\.\[\n(.*?)^    \]\.map\([^\n]*\n(.*?)^    \}\)\),?$)",
        {.dotall = true, .multiline = true});
    static const pt::regex spread_entry(R"(^        \{\n(.*?)^        \},?$)",
        {.dotall = true, .multiline = true});
    const auto colon = spec.rfind(':');
    const auto text = read(v, spec.substr(0, colon));
    const auto name = spec.substr(colon + 1);
    std::optional<std::string> array;
    for (const auto& m : array_re.find_all(text))
        if (m.group(1) == name) {
            array = std::string(m.group(2));
            break;
        }
    std::vector<resource> found;
    if (!array)
        return found;
    for (const auto& e : entry_re.find_all(*array))
        if (auto r = resource_of(members_of(e.group(1), 8)))
            found.push_back(std::move(*r));
    for (const auto& s : spread_re.find_all(*array)) {
        const auto shared = members_of(s.group(2), 8);
        for (const auto& e : spread_entry.find_all(s.group(1)))
            if (auto r = resource_of(members_of(e.group(1), 12) + "\n" + shared))
                found.push_back(std::move(*r));
    }
    return found;
}

std::size_t family_targets(const ct::corpus_view& v, const family& f) {
    std::size_t n = 0;
    for (const auto& r : registry_resources(v, f.spec)) {
        const bool needed = f.needs == "every" ? true
            : f.needs == "one_key" ? r.one_key
            : f.needs == "list_by" ? r.list_by
            : f.needs == "writable" ? r.writable : throw stop{};
        n += needed ? 1 : 0;
    }
    return n;
}

void coverage(const ct::corpus_view& v, const tables& t,
    const std::vector<route>& found, std::vector<std::string>& lines) {
    std::map<std::string, std::pair<int, int>> per_area;
    std::vector<std::string> rows;
    for (const auto& r : found) {
        if (r.server != "bff")
            continue;
        const key2 key{upper(r.method), r.path};
        const auto* d = decide(t, r);
        auto& counts = per_area[d ? d->area : t.no_area];
        ++counts.first;
        const auto pair = t.pairs.find(key);
        const auto fam = t.families.find(key);
        counts.second += pair != t.pairs.end() || fam != t.families.end();
        std::string target = "none";
        if (pair != t.pairs.end())
            target = pair->second.first + " " + pair->second.second;
        else if (fam != t.families.end())
            target = fam->second.method + " " + fam->second.tpl +
                ", one per resource (" +
                std::to_string(family_targets(v, fam->second)) + ")";
        rows.push_back("| " + key.first + " =" + key.second + "= | " + target +
            " |");
    }
    lines.insert(lines.end(), {"* The coverage", "",
        "How many BFF routes each area pairs with a C++ route, counted from",
        "=build/scripts/route_pairing.py=. A pair records the mapping and does not",
        "prove it; the port tasks prove each route against the answer the BFF gives.",
        "", "| Area | BFF routes | Paired | Gaps |",
        "|------+------------+--------+------|"});
    std::vector<std::string> order;
    for (const auto& [area, counts] : per_area)
        order.push_back(area);
    std::ranges::stable_sort(order, [&](const auto& a, const auto& b) {
        return area_before(a, b, t.no_area);
    });
    int total = 0;
    int paired = 0;
    for (const auto& area : order) {
        const auto [n, p] = per_area.at(area);
        total += n;
        paired += p;
        lines.push_back("| " + area + " | " + std::to_string(n) + " | " +
            std::to_string(p) + " | " + std::to_string(n - p) + " |");
    }
    lines.push_back("| total | " + std::to_string(total) + " | " +
        std::to_string(paired) + " | " + std::to_string(total - paired) +
        " |");
    lines.insert(lines.end(), {"", "* The pairing", "",
        "| BFF route | C++ equivalent |", "|-----------+----------------|"});
    lines.insert(lines.end(), rows.begin(), rows.end());
    lines.emplace_back();
}

std::string rstrip(std::string_view s) {
    while (!s.empty() && co::is_space(s.back()))
        s.remove_suffix(1);
    return std::string(s);
}

void run(const ct::corpus_view& v, ct::file_table& out) {
    try {
        const auto t = read_tables(v);
        const auto found = routes(v);
        std::size_t bff = 0;
        for (const auto& r : found)
            bff += r.server == "bff";
        std::vector<std::string> lines{rstrip(t.header), ""};
        area_map(t, found, lines);
        composed_views(v, t, found, lines);
        coverage(v, t, found, lines);
        lines.insert(lines.end(), {"* The inventory", "",
            std::to_string(found.size()) + " routes: " + std::to_string(bff) +
                " answered by the BFF and " +
                std::to_string(found.size() - bff) + " by ores.http.",
            "", "| Kind | Area | Method | Path | Server | Where | Note |",
            "|------+------+--------+------+--------+-------+------|"});
        for (const auto& r : found) {
            const auto* d = decide(t, r);
            std::string note = r.note;
            if (d != nullptr && !d->note.empty())
                note += (note.empty() ? "" : " ") + d->note;
            lines.push_back("| " + (d ? d->kind : "") + " | " +
                (d ? d->area : "") + " | " + r.method + " | =" + r.path +
                "= | " + r.server + " | " + r.where + " | " + note + " |");
        }
        lines.emplace_back();
        out.add(inventory_path, joined(lines, "\n"));
    } catch (const stop&) {
    }
}

}

const ct::descriptor route_inventory{
    .name = "route_inventory",
    .version = 1,
    .summary = "Writes the HTTP route inventory from the routes the BFF and "
               "ores.http register and the route classification and pairing.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = {},
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_table>{run}};

}
