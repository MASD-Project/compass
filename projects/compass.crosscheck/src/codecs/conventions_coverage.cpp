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
#include "compass.crosscheck/codecs/conventions_coverage.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/csv.hpp"
#include "compass.crosscheck/ops/md5.hpp"
#include "compass.crosscheck/ops/python_literals.hpp"
#include "compass.crosscheck/ops/words.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"
#include "compass.platform/text/xml.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::uint32_t duplicate_row = 0;
constexpr std::uint32_t no_source = 1;
constexpr std::uint32_t undefined_index = 2;
constexpr std::uint32_t missing_convention = 3;
constexpr std::uint32_t differing_convention = 4;
constexpr std::uint32_t extract_failed = 5;
constexpr std::uint32_t unreadable = 6;

constexpr std::array<ct::rule, 7> rules{{
    {"duplicate_row", "Each (kind, id) is one row of the conventions "
        "dataset."},
    {"no_source", "Each row of the conventions dataset names its source."},
    {"undefined_index", "Each Ibor or overnight index a convention names is "
        "defined by an index convention of the dataset."},
    {"missing_convention", "The dataset holds every convention ORE holds, "
        "or propose_decisions.tsv says why not."},
    {"differing_convention", "The dataset states a convention's terms as ORE "
        "does, or propose_decisions.tsv says why not."},
    {"extract_failed", "decisions.tsv reads, so the canonical set of the "
        "ORE examples can be extracted."},
    {"unreadable", "Every file and table the check reads is there and "
        "reads."}}};

constexpr std::string_view fix = "Run tools/ore_conventions/extract.py, "
    "then add the convention to conventions.tsv or state why not in "
    "propose_decisions.tsv.";

constexpr std::array<std::string_view, 1> field_columns{"field"};
constexpr std::array<std::string_view, 2> convention_columns{"convention",
    "code"};
constexpr std::array<compass::codec::ops::table_need, 2> tables{{
    {"Index fields", field_columns},
    {"Business day conventions", convention_columns}}};

constexpr std::array<ct::option, 4> options{{
    {"examples", "external/ore/examples",
        "The directory whose *.xml files extract.py reads."},
    {"extract_dir", "tools/ore_conventions",
        "The directory of extract.py, with decisions.tsv and "
        "builtin_index_fields.tsv."},
    {"dataset", "projects/ores.seeder/datasets/conventions",
        "The directory of conventions.tsv, propose_decisions.tsv and "
        "oresmd_index_map.tsv."},
    {compass::codec::ops::data_option, "", "The tables of the check "
        "document whose Definition names this codec: the index fields of "
        "a convention, and the codes of the business day conventions."}}};

using strings = std::vector<std::string>;

/**
 * @brief Where extract.py stops: a DecisionError, with its message.
 */
struct decision_error {
    std::string message;
};

/**
 * @brief Where a script stops with a traceback, writing and reporting
 * nothing more.
 */
struct stop {};

std::string_view local(std::string_view tag) {
    const auto brace = tag.rfind('}');
    return brace == std::string_view::npos ? tag : tag.substr(brace + 1);
}

std::string norm(const std::optional<std::string>& text) {
    return text ? ops::normalised_words(*text) : std::string();
}

std::vector<const pt::xml_element*> by_tag(const pt::xml_document& d,
    const pt::xml_element& e) {
    std::vector<const pt::xml_element*> out;
    for (const auto c : e.children)
        out.push_back(&d[c]);
    std::ranges::stable_sort(out, {}, [](const auto* c) {
        return local(c->tag);
    });
    return out;
}

std::string canon(const pt::xml_document& d, const pt::xml_element& e) {
    const auto text = norm(e.text);
    strings parts;
    if (!text.empty())
        parts.push_back(text);
    for (const auto* c : by_tag(d, e))
        parts.push_back(std::string(local(c->tag)) + "=" + canon(d, *c));
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i)
        out += (i == 0 ? "" : "/") + parts[i];
    return out;
}

std::vector<std::string_view> example_files(const ct::corpus_view& v) {
    return v.list_files(std::string(v.option("examples")) + "/", ".xml");
}

/**
 * @brief Maps one XML file. Its fact says whether its root is
 * Conventions and, if so, each entry: its kind, whether it has an Id, the
 * Id, its signature and its present and non-empty field counts.
 */
void map_file(std::string_view, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    ct::fact_writer fact(out.fact);
    if (!bytes.contains("Conventions")) {
        fact.number(0);
        return;
    }
    const auto doc = pt::xml_document::parse(bytes);
    if (!doc || local(doc->root().tag) != "Conventions") {
        fact.number(0);
        return;
    }
    const auto& root = doc->root();
    fact.number(1);
    fact.number(static_cast<std::uint32_t>(root.children.size()));
    for (const auto c : root.children) {
        const auto& entry = (*doc)[c];
        fact.text(local(entry.tag));
        const pt::xml_element* id = nullptr;
        std::vector<std::pair<std::string, std::string>> fields;
        for (const auto f : entry.children) {
            const auto& field = (*doc)[f];
            const auto tag = local(field.tag);
            if (tag == "Id") {
                if (id == nullptr)
                    id = &field;
                continue;
            }
            fields.emplace_back(tag, canon(*doc, field));
        }
        if (id == nullptr) {
            fact.number(0);
            continue;
        }
        fact.number(1);
        fact.text(norm(id->text));
        std::ranges::stable_sort(fields, {},
            &std::pair<std::string, std::string>::first);
        std::string signature;
        std::uint32_t nonempty = 0;
        for (std::size_t i = 0; i < fields.size(); ++i) {
            signature += (i == 0 ? "" : " ; ") + fields[i].first + "=" +
                fields[i].second;
            if (!fields[i].second.empty())
                ++nonempty;
        }
        fact.text(signature);
        fact.number(static_cast<std::uint32_t>(fields.size()));
        fact.number(nonempty);
    }
}

struct variant {
    std::string signature;
    strings files;
    std::uint32_t nonempty = 0;
};

using key = std::pair<std::string, std::string>;
using definitions = std::map<key, std::map<std::string, variant>>;

constexpr std::array<std::string_view, 5> legacy_markers{"Legacy/",
    "ORE-API/", "ORE-Python/", "ScriptedTrade/", "MinimalSetup/"};

bool is_legacy(std::string_view path) {
    return std::ranges::any_of(legacy_markers, [&](auto m) {
        return path.contains(m);
    });
}

struct decision {
    std::string decision;
    std::string signature;
    std::size_t line = 0;

    [[nodiscard]] bool is_todo() const {
        std::string d(co::strip(decision));
        for (auto& c : d) {
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
        }
        return d.empty() || d == "todo";
    }
};

std::map<key, decision> load_decisions(const std::optional<std::string>&
    bytes) {
    if (!bytes)
        throw decision_error{"no such decisions file"};
    std::map<key, decision> out;
    bool header = false;
    const auto text = pt::universal_newlines(pt::decode_replacing(*bytes));
    std::size_t number = 0;
    for (const auto raw : co::split_lines(text)) {
        ++number;
        auto line = raw;
        while (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (co::strip(line).empty())
            continue;
        auto lead = line;
        while (!lead.empty() && co::is_space(lead.front()))
            lead.remove_prefix(1);
        if (lead.starts_with('#'))
            continue;
        const auto columns = ops::split(line, '\t');
        if (!header) {
            const std::array<std::string_view, 4> expected{"kind", "id",
                "decision", "signature"};
            if (columns.size() != expected.size() ||
                !std::ranges::equal(columns, expected, {}, [](auto c) {
                    return co::strip(c);
                }))
                throw decision_error{"expected the header kind, id, "
                    "decision, signature"};
            header = true;
            continue;
        }
        if (columns.size() != 4)
            throw decision_error{"a decision does not have 4 columns"};
        const std::string kind(co::strip(columns[0]));
        const std::string ident(co::strip(columns[1]));
        if (kind.empty() || ident.empty())
            throw decision_error{"a decision's kind and id must not be "
                "empty"};
        if (out.contains({kind, ident}))
            throw decision_error{"duplicate decision for " + kind + " / " +
                ident};
        out.emplace(key{kind, ident}, decision{std::string(co::strip(
            columns[2])), std::string(co::strip(columns[3])), number});
    }
    if (!header)
        throw decision_error{"no header row found in decisions.tsv"};
    return out;
}

struct resolution {
    std::string kind;
    std::string id;
    std::size_t variants = 0;
    std::string signature;
    std::string source;
    std::string decided_by;
};

/**
 * @brief The rule's winner among @p variants and how it was decided.
 */
std::pair<const variant*, std::string> resolve(
    const std::vector<const variant*>& variants) {
    if (variants.size() == 1)
        return {variants.front(), "only-variant"};
    std::uint32_t most = 0;
    for (const auto* v : variants)
        most = std::max(most, v->nonempty);
    std::vector<const variant*> rule1;
    for (const auto* v : variants) {
        if (v->nonempty == most)
            rule1.push_back(v);
    }
    if (rule1.size() == 1)
        return {rule1.front(), "rule-1"};
    std::size_t files = 0;
    for (const auto* v : rule1)
        files = std::max(files, v->files.size());
    std::vector<const variant*> rule2;
    for (const auto* v : rule1) {
        if (v->files.size() == files)
            rule2.push_back(v);
    }
    if (rule2.size() == 1)
        return {rule2.front(), "rule-2"};
    std::vector<const variant*> non_legacy;
    for (const auto* v : rule2) {
        if (std::ranges::any_of(v->files, [](const auto& f) {
                return !is_legacy(f);
            }))
            non_legacy.push_back(v);
    }
    if (non_legacy.size() == 1)
        return {non_legacy.front(), "rule-3"};
    const auto& pool = non_legacy.empty() ? rule2 : non_legacy;
    return {*std::ranges::min_element(pool, {}, &variant::signature),
        "rule-4"};
}

const variant* match_pin(const decision& d,
    const std::vector<const variant*>& variants) {
    std::vector<const variant*> by_digest;
    for (const auto* v : variants) {
        if (ops::md5_hex(v->signature).substr(0, 10) == d.decision)
            by_digest.push_back(v);
    }
    if (by_digest.size() > 1)
        throw decision_error{"a hash matches several variants"};
    if (!by_digest.empty())
        return by_digest.front();
    std::vector<const variant*> by_signature;
    for (const auto* v : variants) {
        if (v->signature == d.decision)
            by_signature.push_back(v);
    }
    if (by_signature.size() > 1)
        throw decision_error{"a signature matches several variants"};
    return by_signature.empty() ? nullptr : by_signature.front();
}

constexpr std::string_view suffix = "-CONVENTIONS";

std::string_view stem_of(std::string_view ident) {
    return ident.ends_with(suffix) ? ident.substr(0, ident.size() -
        suffix.size()) : ident;
}

/**
 * @brief kind -> {alias: canonical} for each group of Ids of one kind that
 * share one definition and differ only by a trailing -CONVENTIONS.
 */
std::map<std::string, std::map<std::string, std::string>> renames_of(
    const definitions& defs) {
    std::map<std::pair<std::string, std::string>, std::set<std::string>>
        by_signature;
    for (const auto& [k, variants] : defs) {
        for (const auto& [sig, v] : variants)
            by_signature[{k.first, sig}].insert(k.second);
    }
    std::map<std::string, std::map<std::string, std::string>> out;
    for (const auto& [ks, idents] : by_signature) {
        if (idents.size() < 2)
            continue;
        std::set<std::string_view> stems;
        for (const auto& i : idents)
            stems.insert(stem_of(i));
        if (stems.size() != 1)
            continue;
        const auto canonical = std::string(*stems.begin()) +
            std::string(suffix);
        std::vector<std::string_view> suffixed;
        for (const auto& i : idents) {
            if (i.ends_with(suffix))
                suffixed.push_back(i);
        }
        if (suffixed.size() != 1 || suffixed.front() != canonical)
            continue;
        for (const auto& i : idents) {
            if (i != canonical)
                out[ks.first][i] = canonical;
        }
    }
    return out;
}

std::string canonical_table(const definitions& raw,
    std::map<key, decision> decisions) {
    const auto renames = renames_of(raw);
    const auto renamed = [&](const std::string& kind,
                             const std::string& ident) {
        const auto k = renames.find(kind);
        if (k == renames.end())
            return ident;
        const auto a = k->second.find(ident);
        return a == k->second.end() ? ident : a->second;
    };
    std::map<key, std::set<std::string>> aliases;
    for (const auto& [kind, mapping] : renames) {
        for (const auto& [alias, canonical] : mapping)
            aliases[{kind, canonical}].insert(alias);
    }

    definitions defs;
    for (const auto& [k, variants] : raw) {
        auto& bucket = defs[{k.first, renamed(k.first, k.second)}];
        for (const auto& [sig, v] : variants) {
            auto [it, fresh] = bucket.emplace(sig, v);
            if (fresh)
                continue;
            std::set<std::string> files(it->second.files.begin(),
                it->second.files.end());
            files.insert(v.files.begin(), v.files.end());
            it->second.files.assign(files.begin(), files.end());
        }
    }

    std::map<key, decision> moved;
    for (const auto& [k, d] : decisions) {
        const key target{k.first, renamed(k.first, k.second)};
        const auto [it, fresh] = moved.emplace(target, d);
        if (!fresh && (it->second.decision != d.decision ||
                it->second.signature != d.signature))
            throw decision_error{"two decisions rename onto " +
                target.first + " / " + target.second + " and differ"};
    }

    std::set<key> keys;
    for (const auto& [k, _] : defs)
        keys.insert(k);
    for (const auto& [k, _] : moved)
        keys.insert(k);

    std::string out = "kind\tid\tsignature\tsource_path\tvariant_count\t"
        "decided_by\taliases\n";
    for (const auto& k : keys) {
        std::vector<const variant*> variants;
        if (const auto d = defs.find(k); d != defs.end()) {
            for (const auto& [sig, v] : d->second)
                variants.push_back(&v);
        }
        const auto d = moved.find(k);
        const bool pinned = d != moved.end() && !d->second.is_todo();
        const variant* matched = pinned ? match_pin(d->second, variants)
                                        : nullptr;
        resolution r{k.first, k.second, variants.size(), "", "<stale-pin>",
            ""};
        if (matched != nullptr) {
            r.signature = matched->signature;
            r.source = matched->files.empty() ? "<stale-pin>"
                                              : matched->files.front();
            r.decided_by = "pinned";
        } else {
            const variant* winner = nullptr;
            if (!variants.empty()) {
                std::tie(winner, r.decided_by) = resolve(variants);
            } else {
                r.decided_by = "rule-1";
            }
            if (r.decided_by == "only-variant") {
                const auto kr = renames.find(k.first);
                if (kr != renames.end() && std::ranges::any_of(kr->second,
                        [&](const auto& m) { return m.second == k.second; }))
                    r.decided_by = "renamed";
            }
            if (winner != nullptr) {
                r.signature = winner->signature;
                r.source = winner->files.empty() ? "<stale-pin>"
                                                 : winner->files.front();
            }
            if (pinned || (d != moved.end() && variants.empty())) {
                r.signature = d->second.signature;
                r.source = "<stale-pin>";
                r.decided_by = "pinned";
            }
        }
        std::string joined;
        if (const auto a = aliases.find(k); a != aliases.end()) {
            for (const auto& alias : a->second)
                joined += (joined.empty() ? "" : ",") + alias;
        }
        out += r.kind + "\t" + r.id + "\t" + r.signature + "\t" + r.source +
            "\t" + std::to_string(r.variants) + "\t" + r.decided_by + "\t" +
            joined + "\n";
    }
    return out;
}

using row = std::map<std::string, std::optional<std::string>, std::less<>>;

/**
 * @brief propose.py's read_tsv: the lines that do not start with #, read
 * as csv.DictReader reads them, each row with a kind.
 */
std::vector<row> read_tsv(std::string_view text) {
    std::string kept;
    for (const auto line : co::split_lines_keeping_ends(text)) {
        if (!line.starts_with('#'))
            kept += line;
    }
    const auto records = ops::csv_rows(kept, '\t');
    std::vector<row> out;
    if (records.empty())
        return out;
    const auto& header = records.front();
    for (std::size_t i = 1; i < records.size(); ++i) {
        if (records[i].empty())
            continue;
        row r;
        for (std::size_t c = 0; c < header.size(); ++c)
            r.insert_or_assign(header[c], c < records[i].size()
                ? std::optional(records[i][c]) : std::nullopt);
        const auto k = r.find("kind");
        if (k != r.end() && k->second && !k->second->empty())
            out.push_back(std::move(r));
    }
    return out;
}

const std::string& get(const row& r, std::string_view name) {
    const auto it = r.find(name);
    if (it == r.end() || !it->second)
        throw stop{};
    return *it->second;
}

std::string get_or_empty(const row& r, std::string_view name) {
    const auto it = r.find(name);
    return it == r.end() || !it->second ? std::string() : *it->second;
}

std::string text_of(std::string_view bytes) {
    return pt::universal_newlines(pt::decode_replacing(bytes));
}

struct ore_convention {
    std::string origin;
    std::string kind;
    std::string id;
    std::string signature;
};

void reduce(std::span<const ct::scanned_file> scanned,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto here = std::string(v.option("dataset")) + "/";
    const std::string dataset_path = here + "conventions.tsv";
    const std::string extract_dir(v.option("extract_dir"));
    const auto finding = [&](std::uint32_t rule, const std::string& m) {
        co::report(out, dataset_path, 0, rule, m, fix);
    };

    const std::string examples = std::string(v.option("examples")) + "/";
    std::vector<ct::scanned_file> files(scanned.begin(), scanned.end());
    std::ranges::sort(files, co::component_less, &ct::scanned_file::path);
    definitions defs;
    for (const auto& f : files) {
        ct::fact_reader fact(f.fact);
        if (fact.number() == 0)
            continue;
        const std::string rel(f.path.substr(examples.size()));
        const auto entries = fact.number();
        for (std::uint32_t i = 0; i < entries; ++i) {
            const std::string kind(fact.text());
            if (fact.number() == 0)
                continue;
            const std::string ident(fact.text());
            const std::string sig(fact.text());
            fact.number();
            const auto nonempty = fact.number();
            auto& vt = defs[{kind, ident}][sig];
            if (vt.files.empty() && vt.signature.empty()) {
                vt.signature = sig;
                vt.nonempty = nonempty;
            }
            if (std::ranges::find(vt.files, rel) == vt.files.end())
                vt.files.push_back(rel);
        }
    }
    for (auto& [k, variants] : defs) {
        for (auto& [sig, vt] : variants)
            std::ranges::sort(vt.files);
    }

    const auto data = compass::codec::ops::tables_for(
        "conventions_coverage", v.option(compass::codec::ops::data_option),
        tables);
    if (!data) {
        co::report(out, "compass/checks/conventions_coverage.org", 0,
            unreadable, data.error(), "Give the tables the message names.");
        return;
    }
    const auto read = [&](const std::string& path) {
        auto bytes = v.read_file(path);
        if (!bytes)
            co::report(out, path, 0, unreadable, "conventions_coverage "
                "reads this file, and it is missing", "Restore it, or set "
                "the option that names it.");
        return bytes;
    };
    const auto dataset = read(dataset_path);
    const auto propose_decisions = read(here + "propose_decisions.tsv");
    const auto index_map_bytes = read(here + "oresmd_index_map.tsv");
    const auto engine = read(extract_dir + "/builtin_index_fields.tsv");
    if (!dataset || !propose_decisions || !index_map_bytes || !engine)
        return;
    try {
        std::string canonical;
        try {
            canonical = canonical_table(defs, load_decisions(v.read_file(
                extract_dir + "/decisions.tsv")));
        } catch (const decision_error& e) {
            co::report(out, extract_dir + "/decisions.tsv", 0, extract_failed,
                "extract.py stops: " + e.message, fix);
            return;
        }
        const auto& fields_table = *data->find("Index fields");
        std::vector<std::string> index_fields;
        for (const auto& cells : fields_table.rows)
            index_fields.push_back(cells[fields_table.column("field")]);
        const auto& bdc_table = *data->find("Business day conventions");
        std::map<std::string, std::string, std::less<>> bdc;
        for (const auto& cells : bdc_table.rows)
            bdc.insert_or_assign(cells[bdc_table.column("convention")],
                cells[bdc_table.column("code")]);

        const auto rows = read_tsv(text_of(*dataset));
        std::set<key> seen;
        for (const auto& r : rows) {
            const key k{get(r, "kind"), get(r, "id")};
            if (seen.contains(k))
                finding(duplicate_row, k.first + " " + k.second +
                    ": more than one row");
            seen.insert(k);
            if (co::strip(get_or_empty(r, "source")).empty())
                finding(no_source, k.first + " " + k.second +
                    ": names no source");
        }

        std::map<std::string, std::string, std::less<>> index_map;
        // Named, because a range-for keeps alive only the outermost
        // temporary before C++23's P2718, which MSVC lacks, and the lines
        // view this text.
        const auto index_map_text = text_of(*index_map_bytes);
        for (const auto line : co::split_lines(index_map_text)) {
            const auto parts = ops::split(line, '\t');
            if (parts.size() == 2)
                index_map.insert_or_assign(std::string(parts[0]),
                    std::string(parts[1]));
        }
        static const pt::regex tenor(R"re(&tenor=[^&]*)re");
        const auto family = [&](std::string_view uri) {
            return tenor.substitute(uri, "");
        };
        std::set<std::string> defined;
        for (const auto& r : rows) {
            const auto& kind = get(r, "kind");
            if (kind != "IborIndex" && kind != "OvernightIndex")
                continue;
            auto uri = get_or_empty(r, "oresmd_uri");
            if (uri.empty()) {
                const auto it = index_map.find(get(r, "id"));
                uri = it == index_map.end() ? "" : it->second;
            }
            if (!uri.empty())
                defined.insert(family(uri));
        }
        for (const auto& r : rows) {
            std::map<std::string, std::string, std::less<>> fields;
            const auto& signature = get(r, "signature");
            std::size_t start = 0;
            while (start <= signature.size()) {
                auto end = signature.find(" ; ", start);
                if (end == std::string::npos)
                    end = signature.size();
                const auto part = std::string_view(signature).substr(start,
                    end - start);
                if (const auto eq = part.find('=');
                    eq != std::string_view::npos)
                    fields.insert_or_assign(std::string(part.substr(0, eq)),
                        std::string(part.substr(eq + 1)));
                start = end + 3;
            }
            for (const auto& field : index_fields) {
                const auto f = fields.find(field);
                const std::string name(f == fields.end() ? std::string_view()
                    : co::strip(f->second));
                const auto u = index_map.find(name);
                const auto uri = u == index_map.end() ? std::string()
                                                      : u->second;
                if (uri.contains("index=ibor") &&
                    !defined.contains(family(uri)))
                    finding(undefined_index, get(r, "kind") + " " +
                        get(r, "id") + ": names " + name + ", which no Ibor "
                        "or overnight index convention of the dataset "
                        "defines");
            }
        }

        std::vector<ore_convention> ore;
        for (const auto& r : read_tsv(canonical))
            ore.push_back({"ORE examples", get(r, "kind"), get(r, "id"),
                get(r, "signature")});
        for (const auto& r : read_tsv(text_of(*engine))) {
            const bool ibor = get(r, "kind") == "ibor";
            strings parts;
            if (ibor) {
                const auto& convention = get(r, "convention");
                const auto b = bdc.find(convention);
                parts.push_back("BusinessDayConvention=" +
                    (b == bdc.end() ? convention : b->second));
                parts.push_back("DayCounter=" + get(r, "day_counter"));
                parts.push_back("EndOfMonth=" + get(r, "end_of_month"));
            } else {
                parts.push_back("DayCounter=" + get(r, "day_counter"));
            }
            parts.push_back("FixingCalendar=" + get(r, "fixing_calendar"));
            parts.push_back("SettlementDays=" + get(r, "settlement_days"));
            std::ranges::sort(parts);
            std::string signature;
            for (std::size_t i = 0; i < parts.size(); ++i)
                signature += (i == 0 ? "" : " ; ") + parts[i];
            ore.push_back({"ORE engine", ibor ? "IborIndex" :
                "OvernightIndex", get(r, "name"), signature});
        }
        std::set<std::tuple<std::string, std::string, std::string>> decided;
        for (const auto& r : read_tsv(text_of(*propose_decisions)))
            decided.emplace(get(r, "origin"), get(r, "kind"), get(r, "id"));
        std::map<key, std::string> held;
        for (const auto& r : rows)
            held.insert_or_assign({get(r, "kind"), get(r, "id")},
                get(r, "signature"));
        for (const auto& c : ore) {
            if (decided.contains({c.origin, c.kind, c.id}))
                continue;
            const auto h = held.find({c.kind, c.id});
            if (h == held.end())
                finding(missing_convention, c.origin + ": " + c.kind + " " +
                    c.id + " is not in the dataset");
            else if (h->second != c.signature)
                finding(differing_convention, c.origin + ": " + c.kind + " " +
                    c.id + " differs: ORE states '" + c.signature + "', the "
                    "dataset '" + h->second + "'");
        }
    } catch (const stop&) {
    } catch (const ops::csv_error&) {
    }
}

}

const ct::descriptor conventions_coverage{
    .name = "conventions_coverage",
    .version = 2,
    .summary = "Reports each convention ORE holds that the conventions "
        "dataset lacks or states otherwise, and the dataset's own defects.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{example_files, map_file, reduce}};

}
