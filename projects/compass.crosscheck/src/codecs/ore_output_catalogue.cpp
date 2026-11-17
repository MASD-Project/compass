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
#include "compass.crosscheck/codecs/ore_output_catalogue.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/io/format_cache.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/csv.hpp"
#include "compass.crosscheck/ops/python_literals.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;
using ops::python_value;

constexpr std::string_view script_path =
    "projects/ores.ore/scripts/generate_output_catalogue.py";
constexpr std::string_view docs_path =
    "projects/ores.ore/scripts/output_family_docs.py";
constexpr std::string_view files_path = "projects/ores.sql/utility/ore_files.py";
constexpr std::string_view examples = "external/ore/examples/";
constexpr std::string_view samples = "projects/ores.ore/data/samples/";
constexpr std::string_view seed_path =
    "projects/ores.sql/populate/reporting/reporting_file_types_populate.sql";
constexpr std::string_view page_path =
    "doc/knowledge/external/ore_output_formats.org";
constexpr std::string_view data_path =
    "projects/ores.ore/core/src/results/output_catalogue_data.cpp";
constexpr std::string_view data_hpp_path =
    "projects/ores.ore/core/include/ores.ore.core/results/output_catalogue_data.hpp";
constexpr std::string_view page_id = "34C68112-7A34-4557-835C-BDCAA7BB9C2A";

/**
 * @brief Where the script stops without writing, and why when known.
 */
struct stop {
    std::string why;
};

using strings = std::vector<std::string>;
using text_map = std::map<std::string, std::string, std::less<>>;

/**
 * @brief The tables the script keeps as module-level literals.
 */
struct tables {
    text_map convention;
    std::set<std::string, std::less<>> numeric_qualifiers;
    std::set<std::string, std::less<>> risk_factor_families;
    std::set<std::string, std::less<>> notebook_families;
    std::set<std::string, std::less<>> legacy_families;
    std::map<std::string, strings, std::less<>> aliases;
    text_map same_report;
    text_map out_of_scope;
    std::map<std::pair<std::string, std::string>, std::string> kind_overrides;
    strings keys;
    std::map<std::string, strings, std::less<>> family_keys;
    std::vector<std::pair<std::string, std::string>> prefixes;
    std::set<std::string, std::less<>> nulls;
};

std::string read(const ct::corpus_view& v, std::string_view path) {
    auto text = v.read_file(path);
    if (!text)
        throw stop{"cannot read " + std::string(path)};
    return std::move(*text);
}

python_value literal(std::string_view source, std::string_view name,
    const text_map& constants = {}) {
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

strings string_list(const python_value& v) {
    auto s = ops::strings_of(v);
    if (!s)
        throw stop{};
    return *s;
}

text_map text_dict(const python_value& v) {
    text_map out;
    for (const auto& e : v.entries)
        out.insert_or_assign(e.key, text_of(e.value));
    return out;
}

std::set<std::string, std::less<>> string_set(const python_value& v) {
    const auto s = string_list(v);
    return {s.begin(), s.end()};
}

tables read_tables(std::string_view source) {
    tables t;
    t.convention = text_dict(literal(source, "CONVENTION"));
    t.numeric_qualifiers = string_set(literal(source, "NUMERIC_QUALIFIERS"));
    t.risk_factor_families = string_set(literal(source, "RISK_FACTOR_FAMILIES"));
    t.notebook_families = string_set(literal(source, "NOTEBOOK_FAMILIES"));
    t.legacy_families = string_set(literal(source, "LEGACY_FAMILIES"));
    for (const auto& e : literal(source, "ALIASES").entries)
        t.aliases.insert_or_assign(e.key, string_list(e.value));
    t.same_report = text_dict(literal(source, "SAME_REPORT"));
    t.out_of_scope = text_dict(literal(source, "OUT_OF_SCOPE"));
    for (const auto& e : literal(source, "KIND_OVERRIDES").entries) {
        const auto k = string_list({python_value::kind::sequence, {},
            e.key_items, {}});
        if (k.size() != 2)
            throw stop{};
        t.kind_overrides.insert_or_assign({k[0], k[1]}, text_of(e.value));
    }
    t.keys = string_list(literal(source, "KEYS"));
    for (const auto& e : literal(source, "FAMILY_KEYS").entries)
        t.family_keys.insert_or_assign(e.key, string_list(e.value));
    for (const auto& p : literal(source, "PREFIXES").items) {
        const auto pair = string_list(p);
        if (pair.size() != 2)
            throw stop{};
        t.prefixes.emplace_back(pair[0], pair[1]);
    }
    t.nulls = string_set(literal(source, "NULLS"));
    return t;
}

/**
 * @brief The filename_regex of every ORE file: the schemas' entries, then
 * the literal FILES list, as ore_files.FILES orders them.
 */
std::vector<std::pair<std::string, pt::regex>> ore_files(
    std::string_view source) {
    const auto constants = ops::python_string_constants(source);
    const auto rx = [](std::string_view name) {
        auto escaped = pt::escape(name);
        std::string out;
        for (std::size_t i = 0; i < escaped.size(); ++i) {
            if (escaped.compare(i, 2, "\\*") == 0) {
                out += ".*";
                ++i;
            } else {
                out.push_back(escaped[i]);
            }
        }
        return out;
    };
    std::vector<std::pair<std::string, pt::regex>> out;
    const auto add = [&](const std::string& code, strings alternatives) {
        const auto body = alternatives.size() == 1 ? alternatives[0]
            : "(" + [&] {
                  std::string joined;
                  for (std::size_t i = 0; i < alternatives.size(); ++i)
                      joined += (i ? "|" : "") + alternatives[i];
                  return joined;
              }() + ")";
        out.emplace_back(code, pt::regex("^" + body + "$"));
    };
    for (const auto& s : literal(source, "SCHEMAS").items) {
        const auto row = string_list(s);
        if (row.size() != 3)
            throw stop{};
        add(row[0], {rx(row[2])});
    }
    for (const auto& call : literal(source, "FILES", constants).items) {
        if (call.what != python_value::kind::call || call.items.size() < 6)
            throw stop{};
        strings alternatives;
        for (std::size_t i = 5; i < call.items.size(); ++i)
            alternatives.push_back(rx(text_of(call.items[i])));
        for (const auto& e : call.entries)
            if (e.key == "patterns")
                for (const auto& p : string_list(e.value))
                    alternatives.push_back(p);
        add(text_of(call.items[0]), std::move(alternatives));
    }
    return out;
}

struct family_entry {
    std::string family;
    std::string pattern;
    std::vector<strings> headers;
    std::map<std::string, strings, std::less<>> values;
    std::string sample;
    bool synthetic = false;
};

std::pair<std::string, std::string> family_of(const tables& t,
    std::string_view file_name) {
    static const pt::regex suffix(R"(_\d+$)");
    std::string stem(file_name.ends_with(".csv")
        ? file_name.substr(0, file_name.size() - 4) : file_name);
    for (const auto& [prefix, family] : t.prefixes)
        if (stem.starts_with(prefix))
            return {family, prefix + "*.csv"};
    stem = suffix.substitute(stem, "");
    if (const auto s = t.same_report.find(stem); s != t.same_report.end())
        return {s->second, s->second + ".csv"};
    return {stem, stem + ".csv"};
}

/**
 * @brief The csv files under @p dir in os.walk's order with sorted
 * directories and files: a directory's files, then each subdirectory's.
 */
std::vector<std::string_view> walk(const ct::corpus_view& v,
    const std::string& dir) {
    auto paths = v.list_files(dir + "/", ".csv");
    const auto split = [](std::string_view p) {
        std::vector<std::string_view> parts;
        for (auto slash = p.find('/'); slash != std::string_view::npos;
             slash = p.find('/')) {
            parts.push_back(p.substr(0, slash));
            p.remove_prefix(slash + 1);
        }
        parts.push_back(p);
        return parts;
    };
    std::ranges::sort(paths, [&](std::string_view a, std::string_view b) {
        const auto pa = split(a);
        const auto pb = split(b);
        const auto da = pa.size() - 1;
        const auto db = pb.size() - 1;
        for (std::size_t i = 0; i < std::min(da, db); ++i)
            if (pa[i] != pb[i])
                return pa[i] < pb[i];
        if (da != db)
            return da < db;
        return pa.back() < pb.back();
    });
    return paths;
}

std::map<std::string, family_entry> collect(const ct::corpus_view& v,
    const tables& t) {
    struct source {
        std::string dir;
        const std::set<std::string, std::less<>>* only;
        bool synthetic;
    };
    std::vector<source> sources;
    std::set<std::string> suites;
    const auto top = v.list_files(examples);
    for (const auto path : top) {
        const auto rest = path.substr(examples.size());
        if (const auto slash = rest.find('/'); slash != std::string_view::npos)
            suites.insert(std::string(rest.substr(0, slash)));
    }
    for (const auto& suite : suites)
        sources.push_back({std::string(examples) + suite + "/ExpectedOutput",
            nullptr, false});
    for (const auto& [folder, only] : {
             std::pair{std::string("ORE-Python/Notebooks"), &t.notebook_families},
             std::pair{std::string("Legacy"), &t.legacy_families}}) {
        const auto parent = std::string(examples) + folder + "/";
        std::set<std::string> names;
        for (const auto path : v.list_files(parent)) {
            const auto rest = path.substr(parent.size());
            if (const auto slash = rest.find('/'); slash != std::string_view::npos)
                names.insert(std::string(rest.substr(0, slash)));
        }
        if (names.empty())
            throw stop{};
        for (const auto& name : names)
            sources.push_back({parent + name + "/ExpectedOutput", only, false});
    }
    sources.push_back({std::string(samples) + "captured", nullptr, false});
    sources.push_back({std::string(samples) + "synthetic", nullptr, true});

    std::map<std::string, family_entry> families;
    for (const auto& src : sources) {
        for (const auto path : walk(v, src.dir)) {
            const auto name = path.substr(path.rfind('/') + 1);
            auto [family, pattern] = family_of(t, name);
            if (src.only != nullptr && !src.only->contains(family))
                continue;
            if (std::string_view(src.dir).starts_with(samples) &&
                families.contains(family))
                continue;
            auto [it, fresh] = families.try_emplace(family);
            auto& entry = it->second;
            if (fresh) {
                entry.family = family;
                entry.pattern = pattern;
                entry.sample = std::string(path);
                entry.synthetic = src.synthetic;
            }
            const auto text = pt::decode_ignoring(read(v, path));
            const auto cut = text.find('\n');
            std::string first = text.substr(0, cut);
            const std::string rest = cut == std::string::npos ? std::string()
                : text.substr(cut + 1);
            if (first.starts_with('#'))
                first.erase(0, 1);
            const auto rows = ops::csv_rows(first + "\n" + rest, ',');
            const strings fields = rows.empty() ? strings{} : rows.front();
            std::map<std::string, strings, std::less<>> values;
            for (std::size_t r = 1; r < rows.size(); ++r)
                for (std::size_t i = 0; i < fields.size(); ++i)
                    if (i < rows[r].size())
                        values[fields[i]].push_back(rows[r][i]);
            if (std::ranges::find(entry.headers, fields) == entry.headers.end())
                entry.headers.push_back(fields);
            for (const auto& name_ : fields) {
                const auto found = values.find(name_);
                if (found == values.end())
                    continue;
                auto& into = entry.values[name_];
                const auto n = std::min<std::size_t>(found->second.size(), 50);
                into.insert(into.end(), found->second.begin(),
                    found->second.begin() + static_cast<std::ptrdiff_t>(n));
                values.erase(found);
            }
        }
    }
    return families;
}

std::string kind_of(const tables& t, const strings& values, bool key) {
    static const pt::regex date(R"(^\d{4}-\d{2}-\d{2}$)");
    static const pt::regex time(R"(^\d{1,2}:\d{2}(:\d{2})?$)");
    static const pt::regex integer(R"(^-?\d+$)");
    static const pt::regex decimal(R"(^-?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?$)");
    strings seen;
    for (const auto& value : values)
        if (!t.nulls.contains(value))
            seen.push_back(value);
    if (seen.empty())
        return "text";
    const auto all = [&](const pt::regex& r) {
        return std::ranges::all_of(seen, [&](const std::string& s) {
            return r.match(s).has_value();
        });
    };
    if (all(date))
        return "date";
    if (all(time))
        return "time";
    if (key)
        return "text";
    if (all(integer))
        return "integer";
    if (all(decimal))
        return "decimal";
    return "text";
}

using seed_map = std::map<std::string,
    std::vector<std::pair<std::string, std::string>>, std::less<>>;

seed_map seed_outputs(const ct::corpus_view& v) {
    static const pt::regex pattern(
        R"(\(\s*'([^']+)',\s*'([^']*)',\s*'([^']*)',\s*'output',\s*'[^']*',\s*'[^']*',)"
        R"(\s*(?:null|'[^']*'),\s*'([^']*)',\s*(\d+),\s*(\d+),)");
    const auto text = pt::universal_newlines(read(v, seed_path));
    seed_map out;
    for (const auto& m : pattern.find_all(text))
        out[std::string(m.group(4))].emplace_back(m.group(1), m.group(2));
    return out;
}

struct column {
    std::string name;
    std::string kind;
};

struct row {
    std::string family;
    std::string pattern;
    std::string parameter;
    std::string analytic;
    strings keys;
    strings required;
    strings qualifiers;
    strings measures;
    bool open = false;
    std::size_t variants = 0;
    std::string sample;
    strings aliases;
    bool synthetic = false;
    std::vector<column> columns;
};

std::string joined(const strings& parts, std::string_view sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i)
        out += (i ? std::string(sep) : std::string()) + parts[i];
    return out;
}

bool contains(const strings& s, std::string_view x) {
    return std::ranges::find(s, x) != s.end();
}

std::vector<row> rows_of(const tables& t,
    const std::map<std::string, family_entry>& families, const seed_map& seed,
    const std::vector<std::pair<std::string, pt::regex>>& files) {
    std::vector<row> out;
    for (const auto& [family, entry] : families) {
        const auto& headers = entry.headers;
        strings names;
        for (const auto& h : headers)
            for (const auto& name : h)
                if (!name.empty() && !contains(names, name))
                    names.push_back(name);
        std::set<std::string> required;
        for (const auto& name : names)
            if (std::ranges::all_of(headers, [&](const strings& h) {
                    return contains(h, name);
                }))
                required.insert(name);
        strings candidates = t.keys;
        if (const auto f = t.family_keys.find(family); f != t.family_keys.end())
            candidates.insert(candidates.end(), f->second.begin(),
                f->second.end());
        row r;
        for (const auto& k : candidates)
            if (!contains(r.keys, k) && required.contains(k))
                r.keys.push_back(k);
        for (const auto& name : names) {
            std::string kind;
            if (const auto o = t.kind_overrides.find({family, name});
                o != t.kind_overrides.end() && !o->second.empty())
                kind = o->second;
            else {
                const auto values = entry.values.find(name);
                kind = kind_of(t, values == entry.values.end() ? strings{}
                    : values->second, contains(r.keys, name));
            }
            r.columns.push_back({name, kind});
            if (contains(r.keys, name))
                continue;
            if (kind == "text" || kind == "date" || kind == "time" ||
                t.numeric_qualifiers.contains(name))
                r.qualifiers.push_back(name);
            else
                r.measures.push_back(name);
        }
        std::set<std::string> textual;
        for (const auto& c : r.columns)
            if (c.kind == "text" || c.kind == "date" || c.kind == "time")
                textual.insert(c.name);
        std::set<std::set<std::string>> measure_sets;
        for (const auto& h : headers) {
            std::set<std::string> m;
            for (const auto& n : h)
                if (!contains(r.keys, n) && !textual.contains(n) &&
                    !t.numeric_qualifiers.contains(n))
                    m.insert(n);
            measure_sets.insert(std::move(m));
        }
        r.open = measure_sets.size() > 1 ||
            t.risk_factor_families.contains(family);
        if (r.open)
            for (const auto& m : r.measures)
                required.erase(m);
        if (const auto s = seed.find(entry.pattern); s != seed.end() &&
            !s->second.empty()) {
            r.parameter = s->second.front().first;
            std::set<std::string> analytics;
            for (const auto& [code, analytic] : s->second)
                analytics.insert(analytic);
            r.analytic = joined({analytics.begin(), analytics.end()}, ",");
        } else if (const auto c = t.convention.find(family);
                   c != t.convention.end()) {
            r.analytic = c->second;
        }
        std::string name = entry.pattern;
        for (std::size_t at = name.find('*'); at != std::string::npos;
             at = name.find('*', at + 3))
            name.replace(at, 1, "KEY");
        strings hits;
        for (const auto& [code, regex] : files)
            if (regex.fullmatch(name))
                hits.push_back(code);
        if (hits.size() != 1)
            throw stop{};
        r.family = hits.front();
        r.pattern = entry.pattern;
        r.required = {required.begin(), required.end()};
        r.variants = headers.size();
        r.sample = entry.sample;
        if (const auto a = t.aliases.find(family); a != t.aliases.end())
            r.aliases = a->second;
        r.synthetic = entry.synthetic;
        out.push_back(std::move(r));
    }
    return out;
}

/**
 * @brief fnmatch.fnmatch on a POSIX name: * and ? and bracket classes.
 */
bool fnmatch(std::string_view name, std::string_view pattern) {
    std::string re = "^";
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        const char c = pattern[i];
        if (c == '*')
            re += ".*";
        else if (c == '?')
            re += ".";
        else if (c == '[' && pattern.find(']', i + 1) != std::string_view::npos) {
            const auto close = pattern.find(']', i + 1);
            auto inside = std::string(pattern.substr(i + 1, close - i - 1));
            if (!inside.empty() && inside[0] == '!')
                inside[0] = '^';
            re += "[" + inside + "]";
            i = close;
        } else
            re += pt::escape(std::string_view(&c, 1));
    }
    return pt::regex(re + "\\z", {.dotall = true}).match(name).has_value();
}

constexpr std::string_view data_hpp = R"(/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generator: projects/ores.ore/scripts/generate_output_catalogue.py
 * To modify, update the generator and regenerate.
 */
#ifndef ORES_ORE_CORE_RESULTS_OUTPUT_CATALOGUE_DATA_HPP
#define ORES_ORE_CORE_RESULTS_OUTPUT_CATALOGUE_DATA_HPP

#include <string_view>

namespace ores::ore::results {

/**
 * @brief Every output file ORE writes, one line for each family.
 *
 * The lines are the data behind output_catalogue.hpp. Only the parser there
 * reads them.
 */
extern const std::string_view output_catalogue_text;

}

#endif
)";

constexpr std::string_view data_head = R"(/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Generator: projects/ores.ore/scripts/generate_output_catalogue.py
 * To modify, update the generator and regenerate.
 *
 * Every output file ORE writes, as its samples state it. One line for each
 * family, its fields separated by a tab: family, file pattern, the parameter
 * that names it, the analytics that write it, its key columns, how many header
 * variants the samples show, a sample file, the other names the family is
 * written under, whether its only sample is synthetic, then each column with
 * its kind.
 */

#include "ores.ore.core/results/output_catalogue_data.hpp"

namespace ores::ore::results {

const std::string_view output_catalogue_text{
)";

std::string render(const std::vector<row>& catalogue) {
    std::string text = "\n";
    for (std::size_t i = 0; i < catalogue.size(); ++i) {
        const auto& r = catalogue[i];
        strings cols;
        for (const auto& c : r.columns)
            cols.push_back(c.name + ":" + c.kind);
        if (i > 0)
            text += "\n";
        text += joined({r.family, r.pattern, r.parameter, r.analytic,
            joined(r.keys, ","), joined(r.required, ","),
            joined(r.qualifiers, ","), joined(r.measures, ","),
            r.open ? "1" : "0", std::to_string(r.variants), r.sample,
            joined(r.aliases, ","), r.synthetic ? "1" : "0",
            joined(cols, ",")}, "\t");
    }
    text += "\n";
    std::string delimiter = "CAT";
    for (int index = 1; text.find(")" + delimiter + "\"") != std::string::npos;
         ++index)
        delimiter = "CAT" + std::to_string(index);
    strings literals;
    const auto total = co::characters(text);
    for (std::size_t start = 0; start < total; start += 4000) {
        const auto from = co::byte_of_character(text, start);
        const auto to = co::byte_of_character(text, start + 4000);
        literals.push_back("R\"" + delimiter + "(" +
            text.substr(from, to - from) + ")" + delimiter + "\"");
    }
    return std::string(data_head) + joined(literals, "\n") + "\n};\n\n}\n";
}

std::string cell(std::string_view text) {
    std::string out;
    for (const char c : text)
        out += c == '|' ? std::string("\\vert{}") : std::string(1, c);
    return out;
}

std::string page(const std::vector<row>& catalogue, const tables& t,
    const std::map<std::string, std::pair<std::string, std::string>,
        std::less<>>& docs) {
    std::set<std::string> families;
    for (const auto& r : catalogue)
        families.insert(r.family);
    for (const auto& f : families)
        if (!docs.contains(f))
            throw stop{};
    for (const auto& [f, d] : docs)
        if (!families.contains(f))
            throw stop{};
    strings out{":PROPERTIES:", ":ID: " + std::string(page_id), ":END:",
        "#+title: ORE output formats",
        "#+description: Every output file ORE writes, as ORE Studio reads it: "
        "the file, the parameter that names it, the analytic that writes it, "
        "its key and every column with its kind, what each figure measures "
        "and its unit, and a sample that proves it.",
        "#+type: knowledge", "#+level: cross",
        "#+filetags: :ore:results:external:knowledge:",
        "#+created: 2026-11-02", "#+updated: 2026-11-02", "",
        "This page is generated by "
        "=projects/ores.ore/scripts/generate_output_catalogue.py= from the "
        "output catalogue and =output_family_docs.py=; edit those and "
        "regenerate, never the page. The generator's =--check= fails when the "
        "page and the catalogue disagree.",
        "", "* Summary", "",
        "ORE publishes no list of what its output files hold. ORE Studio's "
        "catalogue names " + std::to_string(catalogue.size()) +
        " families, derived from the samples ORE ships and checked against "
        "the writers of the vendored version. Each family below states the "
        "file it is written to, the parameter that names it, the analytic "
        "that writes it, the columns that identify a row, every column with "
        "its kind, what its figures measure and their unit, and the sample "
        "that proves the readers read it. How the files are read and where "
        "the rows are stored is in "
        "[[id:8BF75172-CD91-461A-A3BA-2B4E913D63A8][ORE output processing]].",
        "", "* Detail", "", "** How to read a family", "",
        "- *Key* columns identify a row; a reader refuses a row that leaves "
        "its subject empty.",
        "- *Qualifier* columns state the context of a figure and travel with "
        "it.",
        "- *Measure* columns are the figures; each becomes one stored value "
        "named by its heading.",
        "- An *open* family gains a measure column for each curve, factor or "
        "quantile the run asks for, so its measures here are a sample of what "
        "a run states, and none is required.",
        "- *Other names* are the names a run writes the family under when its "
        "configuration names none.",
        "- A *synthetic* sample is one ORE Studio wrote from the writer's "
        "columns because ORE ships none; it proves the columns, not real "
        "figures.",
        "", "** Outputs not read into rows", "", "| File | Why |",
        "|------+-----|"};
    for (const auto& [name, why] : t.out_of_scope)
        out.push_back("| =" + cell(name) + "= | " + cell(why) + " |");
    out.insert(out.end(), {"", "** Families", ""});
    for (const auto& r : catalogue) {
        const auto& [meaning, units] = docs.find(r.family)->second;
        out.insert(out.end(), {"*** " + r.family, "", meaning, "",
            "| Field | Value |", "|-------+-------|",
            "| File | =" + cell(r.pattern) + "= |"});
        if (!r.aliases.empty()) {
            strings names;
            for (const auto& a : r.aliases)
                names.push_back("=" + cell(a) + "=");
            out.push_back("| Other names | " + joined(names, ", ") + " |");
        }
        out.push_back("| Parameter | " + (r.parameter.empty()
            ? std::string("none; the engine names it")
            : "=" + cell(r.parameter) + "=") + " |");
        const auto analytics = cell(joined(
            [&] {
                strings a;
                std::string_view s = r.analytic;
                while (!s.empty()) {
                    const auto comma = s.find(',');
                    a.emplace_back(s.substr(0, comma));
                    s = comma == std::string_view::npos ? std::string_view{}
                        : s.substr(comma + 1);
                }
                return a;
            }(), ", "));
        out.push_back("| Analytics | " + (analytics.empty()
            ? std::string("none stated") : analytics) + " |");
        const auto keys = cell(joined(r.keys, ", "));
        out.push_back("| Key | " + (keys.empty()
            ? std::string("none: one row for each line") : keys) + " |");
        out.push_back(std::string("| Measures | ") + (r.open
            ? "open: the run decides them" : "closed") + " |");
        out.push_back("| Units | " + cell(units) + " |");
        out.push_back("| Header variants | " + std::to_string(r.variants) +
            " |");
        out.push_back("| Sample | =" + cell(r.sample) + "=" +
            (r.synthetic ? " (synthetic)" : "") + " |");
        out.insert(out.end(), {"", "| Column | Kind | Role |",
            "|--------+------+------|"});
        for (const auto& c : r.columns) {
            const auto role = contains(r.keys, c.name) ? "key"
                : contains(r.qualifiers, c.name) ? "qualifier"
                : contains(r.measures, c.name) ? "measure" : "";
            out.push_back("| " + cell(c.name) + " | " + cell(c.kind) + " | " +
                role + " |");
        }
        out.emplace_back();
    }
    out.insert(out.end(), {"* See also", "",
        "- [[id:8BF75172-CD91-461A-A3BA-2B4E913D63A8][ORE output processing]] "
        "— how the files are read, and why their shape follows the "
        "configuration.",
        "- [[id:D697A04C-387C-451E-AC90-1989FCDFCAB9][Report post-processing]] "
        "— which store takes which shape.",
        "- [[id:969C14D2-4280-4D7C-8F51-1D395B521C83][Journey: read what a run "
        "produced]] — the reader's view of the same files.",
        "- [[id:CF2C4F3A-9C5C-4E7A-9E16-2A3A8C1D4C11][External knowledge]]",
        ""});
    return joined(out, "\n");
}

void run(const ct::corpus_view& v, ct::file_output& out) {
    try {
        const auto t = read_tables(read(v, script_path));
        const auto files = ore_files(read(v, files_path));
        const auto families = collect(v, t);
        const auto seed = seed_outputs(v);
        const auto catalogue = rows_of(t, families, seed, files);
        strings patterns;
        for (const auto& r : catalogue) {
            patterns.push_back(r.pattern);
            patterns.insert(patterns.end(), r.aliases.begin(), r.aliases.end());
        }
        for (const auto& [name, rows] : seed) {
            if (t.out_of_scope.contains(name))
                continue;
            if (std::ranges::none_of(patterns, [&](const std::string& p) {
                    return fnmatch(name, p);
                }))
                throw stop{"the ORE output " + name + " matches no pattern "
                    "of the catalogue"};
        }
        const auto docs_source = read(v, docs_path);
        const auto constants = ops::python_string_constants(docs_source);
        std::map<std::string, std::pair<std::string, std::string>,
            std::less<>> docs;
        for (const auto& e : literal(docs_source, "FAMILY_DOCS",
                 constants).entries) {
            const auto pair = string_list(e.value);
            if (pair.size() != 2)
                throw stop{};
            docs.insert_or_assign(e.key, std::pair{pair[0], pair[1]});
        }
        const auto formats_page = page(catalogue, t, docs);
        out.files.add(data_path, render(catalogue));
        out.files.add(data_hpp_path, data_hpp);
        out.files.add(page_path, formats_page);
    } catch (const stop& s) {
        co::refuse(out, data_path, s.why.empty() ? "its inputs do not read "
            "as " + std::string(script_path) + " reads them" : s.why,
            "Run " + std::string(script_path) + ", which names the problem, "
            "and correct the input it names.");
    } catch (const ops::csv_error& e) {
        co::refuse(out, data_path, std::string("an ORE output sample is not "
            "CSV: ") + e.what(), "Correct the sample the reason names.");
    }
}

constexpr std::array<ct::option, 2> options{{
    {"clang_format", "clang-format",
        "The clang-format the generator runs over the data files, a program "
        "on PATH or a path under the checkout; empty for none."},
    {"clang_format_style", ".clang-format",
        "The style file under the checkout clang-format reads."}}};

std::vector<ct::formatter> formatters_of(const ct::option_values& o,
    const std::filesystem::path& checkout) {
    const auto name = o["clang_format"];
    if (name.empty())
        return {};
    const auto program = name.find('/') == std::string_view::npos
        ? compass::codec::io::find_program(name)
        : compass::codec::io::find_program((checkout / name).string());
    if (!program)
        throw std::runtime_error("ore_output_catalogue: " + std::string(name) +
            " is not a program, for the option clang_format");
    return {{.name = "clang-format", .program = *program,
        .extensions = {".cpp", ".hpp"},
        .arguments = {"-i", "--style=file:{style}"}, .passes = 1,
        .style = checkout / std::string(o["clang_format_style"])}};
}

}

const ct::descriptor ore_output_catalogue{
    .name = "ore_output_catalogue",
    .version = 1,
    .summary = "Writes the ORE output catalogue data and the output formats "
               "page from the output samples ORE ships.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = co::refusal_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{run},
    .formatters = formatters_of};

}
