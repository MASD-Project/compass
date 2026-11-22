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
#include <array>
#include <expected>
#include <format>
#include <stdexcept>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;
namespace cops = compass::codec::ops;

constexpr std::string_view codec_name = "route_inventory";
constexpr std::string_view check_document =
    "compass/checks/route_inventory.org";

constexpr std::uint32_t unreadable = 1;

constexpr std::array<ct::rule, 2> rules{{
    co::refusal_rules[0],
    {"unreadable", "The check document gives every table and column the "
        "inventory is written from, and each row reads: a setting the codec "
        "has, a match it knows, a regex that compiles, a need it knows, and "
        "one row per key; and the configuration names the inventory's "
        "page."}}};

constexpr std::array<std::string_view, 1> head_columns{"line"};
constexpr std::array<std::string_view, 2> setting_columns{"setting",
    "value"};
constexpr std::array<std::string_view, 7> rule_columns{"name", "match",
    "pattern", "server", "kind", "area", "note"};
constexpr std::array<std::string_view, 7> exemption_columns{"server",
    "method", "path", "name", "kind", "area", "note"};
constexpr std::array<std::string_view, 4> view_columns{"view", "owner",
    "computation", "cache policy"};
constexpr std::array<std::string_view, 4> pair_columns{"BFF method",
    "BFF path", "C++ method", "C++ path"};
constexpr std::array<std::string_view, 7> family_columns{"BFF method",
    "BFF path", "registry", "array", "C++ method", "C++ path", "needs"};

constexpr std::array<cops::table_need, 7> needed{{
    {"Head", head_columns},
    {"Settings", setting_columns},
    {"Rules", rule_columns},
    {"Exemptions", exemption_columns},
    {"Views", view_columns},
    {"Pairs", pair_columns},
    {"Families", family_columns}}};

constexpr std::array<std::string_view, 4> needs{"every", "one_key",
    "list_by", "writable"};

/**
 * @brief Where the codec stops without writing, and why.
 */
struct stop {
    std::string why;
};

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

/**
 * @brief A BFF route that serves every resource of a registry at one
 * address: the registry file and its array, the C++ method and address,
 * and which resources must answer it.
 */
struct family {
    std::string registry;
    std::string array;
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

std::string read(const ct::corpus_view& v, std::string_view path) {
    auto text = v.read_file(path);
    if (!text)
        throw stop{"cannot read " + std::string(path)};
    return std::move(*text);
}

/**
 * @brief The tables of the check document, or why a row does not read,
 * written for the reader of the finding.
 */
std::expected<tables, std::string> read_document(
    const cops::data_tables& data) {
    const auto wrong = [&](const cops::data_table& table, std::uint32_t line,
                           const std::string& what) {
        return std::unexpected(std::format("{}:{}: {}, in the table \"{}\" "
            "{} reads", data.source, line, what, table.name, codec_name));
    };
    const auto cell = [](const cops::data_table& table, std::size_t r,
                          std::string_view column) -> const std::string& {
        return table.rows[r][table.column(column)];
    };
    tables out;

    const auto& h = *data.find("Head");
    std::vector<std::string> head;
    for (std::size_t r = 0; r < h.rows.size(); ++r)
        head.push_back(cell(h, r, "line"));
    out.header = joined(head, "\n");

    const auto& s = *data.find("Settings");
    const std::array<std::pair<std::string_view, std::string*>, 2> settings{{
        {"composed kind", &out.composed}, {"no area", &out.no_area}}};
    std::set<std::string, std::less<>> seen;
    for (std::size_t r = 0; r < s.rows.size(); ++r) {
        const auto& name = cell(s, r, "setting");
        const auto key = std::ranges::find(settings, name,
            &std::pair<std::string_view, std::string*>::first);
        if (key == settings.end())
            return wrong(s, s.row_lines[r], "the setting '" + name +
                "' is not one " + std::string(codec_name) + " has");
        if (!seen.insert(name).second)
            return wrong(s, s.row_lines[r], "the setting '" + name +
                "' is stated twice");
        if (cell(s, r, "value").empty())
            return wrong(s, s.row_lines[r], "the setting '" + name +
                "' has no value");
        *key->second = cell(s, r, "value");
    }
    for (const auto& [name, _] : settings)
        if (!seen.contains(name))
            return wrong(s, s.line, "no row states the setting '" +
                std::string(name) + "'");

    const auto& rl = *data.find("Rules");
    for (std::size_t r = 0; r < rl.rows.size(); ++r) {
        rule x;
        x.d = {cell(rl, r, "name"), cell(rl, r, "kind"), cell(rl, r, "area"),
            cell(rl, r, "note")};
        x.shape = cell(rl, r, "match");
        x.pattern = cell(rl, r, "pattern");
        if (const auto& server = cell(rl, r, "server"); !server.empty())
            x.server = server;
        if (x.shape == "regex") {
            try {
                x.compiled = std::make_shared<pt::regex>(x.pattern);
            } catch (const std::invalid_argument& e) {
                return wrong(rl, rl.row_lines[r], "the pattern of the rule '" +
                    x.d.name + "' does not compile: " + e.what());
            }
        } else if (x.shape != "exact" && x.shape != "prefix") {
            return wrong(rl, rl.row_lines[r], "the rule '" + x.d.name +
                "' matches by '" + x.shape + "', not exact, prefix or regex");
        }
        out.rules.push_back(std::move(x));
    }

    const auto& e = *data.find("Exemptions");
    for (std::size_t r = 0; r < e.rows.size(); ++r) {
        key3 key{cell(e, r, "server"), cell(e, r, "method"),
            cell(e, r, "path")};
        if (!out.exceptions.emplace(std::move(key), decision{
                cell(e, r, "name"), cell(e, r, "kind"), cell(e, r, "area"),
                cell(e, r, "note")}).second)
            return wrong(e, e.row_lines[r], "the route " + cell(e, r,
                "method") + " " + cell(e, r, "path") + " of " + cell(e, r,
                "server") + " is exempted twice");
    }

    const auto& w = *data.find("Views");
    for (std::size_t r = 0; r < w.rows.size(); ++r) {
        if (!out.views.emplace(cell(w, r, "view"), std::vector<std::string>{
                cell(w, r, "owner"), cell(w, r, "computation"),
                cell(w, r, "cache policy")}).second)
            return wrong(w, w.row_lines[r], "the view '" + cell(w, r, "view") +
                "' is stated twice");
    }

    const auto& p = *data.find("Pairs");
    for (std::size_t r = 0; r < p.rows.size(); ++r) {
        if (!out.pairs.emplace(key2{cell(p, r, "BFF method"),
                cell(p, r, "BFF path")}, key2{cell(p, r, "C++ method"),
                cell(p, r, "C++ path")}).second)
            return wrong(p, p.row_lines[r], "the BFF route " + cell(p, r,
                "BFF method") + " " + cell(p, r, "BFF path") +
                " is paired twice");
    }

    const auto& f = *data.find("Families");
    for (std::size_t r = 0; r < f.rows.size(); ++r) {
        family x{cell(f, r, "registry"), cell(f, r, "array"),
            cell(f, r, "C++ method"), cell(f, r, "C++ path"),
            cell(f, r, "needs")};
        const auto route = cell(f, r, "BFF method") + " " +
            cell(f, r, "BFF path");
        if (x.registry.empty() || x.array.empty())
            return wrong(f, f.row_lines[r], "the family " + route +
                " names no registry or array");
        if (!std::ranges::contains(needs, x.needs))
            return wrong(f, f.row_lines[r], "the family " + route +
                " needs '" + x.needs + "', not every, one_key, list_by or "
                "writable");
        if (!out.families.emplace(key2{cell(f, r, "BFF method"),
                cell(f, r, "BFF path")}, std::move(x)).second)
            return wrong(f, f.row_lines[r], "the family " + route +
                " is stated twice");
    }
    return out;
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

/**
 * @brief Areas in the inventory's order: the no-area setting last, the
 * rest by name.
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
        if (const auto o = t.views.find(name); o != t.views.end())
            obligations = o->second;
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
    const family& f) {
    static const pt::regex array_re(R"(^(?:export )?const (\w+)\b[^=]*= \[\n(.*?)^\];)",
        {.dotall = true, .multiline = true});
    static const pt::regex entry_re(R"(^    \{\n(.*?)^    \},?$)",
        {.dotall = true, .multiline = true});
    static const pt::regex spread_re(
        R"(^    \.\.\.\[\n(.*?)^    \]\.map\([^\n]*\n(.*?)^    \}\)\),?$)",
        {.dotall = true, .multiline = true});
    static const pt::regex spread_entry(R"(^        \{\n(.*?)^        \},?$)",
        {.dotall = true, .multiline = true});
    const auto text = read(v, f.registry);
    const auto& name = f.array;
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
    for (const auto& r : registry_resources(v, f)) {
        const bool answers = f.needs == "every" ||
            (f.needs == "one_key" && r.one_key) ||
            (f.needs == "list_by" && r.list_by) ||
            (f.needs == "writable" && r.writable);
        n += answers ? 1 : 0;
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
        "How many BFF routes each area pairs with a C++ route, counted from the",
        "Pairs and Families tables of the route inventory's check document. A pair",
        "records the mapping and does not prove it; the port tasks prove each route",
        "against the answer the BFF gives.",
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

void run(const ct::corpus_view& v, ct::file_output& out) {
    const auto data = cops::tables_for(codec_name, v.option(cops::data_option),
        needed);
    if (!data) {
        co::report(out.refusals, check_document, 0, unreadable, data.error(),
            "Give the tables the message names.");
        return;
    }
    const auto document = read_document(*data);
    if (!document) {
        co::report(out.refusals, data->source, 0, unreadable,
            document.error(), "Correct the row the message names.");
        return;
    }
    const auto inventory_path = v.option("page");
    if (inventory_path.empty()) {
        co::report(out.refusals, ".compass.org", 0, unreadable,
            "route_inventory: the option 'page' names no page; set it to "
            "the path of the inventory's page", "Set the option in the "
            "codec's row of .compass.org.");
        return;
    }
    const auto& t = *document;
    try {
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
        out.files.add(inventory_path, joined(lines, "\n"));
    } catch (const stop& s) {
        co::refuse(out, inventory_path, s.why, "Restore the file, or correct "
            "the family that names it in " + data->source + ".");
    }
}

constexpr std::array<ct::option, 2> options{{
    {"page", "", "The path of the page the codec writes whole, its header "
        "from the Head table. A path, not an :ID:: the page is generated "
        "from the check document and carries no ID to look up."},
    {cops::data_option, "", "The tables of the check document whose "
        "Definition names this codec: the inventory's head, the kind and "
        "area the codec gives a meaning to, the rules and exemptions that "
        "decide each route, the composed views' obligations, and the pairs "
        "and families that pair BFF routes with C++ ones."}}};

}

const ct::descriptor route_inventory{
    .name = "route_inventory",
    .version = 4,
    .summary = "Writes the HTTP route inventory from the routes the BFF and "
               "ores.http register and the decisions, views, pairs and "
               "families of its check document.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{run}};

}
