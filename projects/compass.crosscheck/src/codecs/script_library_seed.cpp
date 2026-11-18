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
#include "compass.crosscheck/codecs/script_library_seed.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"
#include "compass.platform/text/xml.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::string_view library_path =
    "external/ore/examples/Products/Input/scriptlibrary.xml";
constexpr std::string_view seed_path =
    "projects/ores.sql/populate/trading/trading_script_library_populate.sql";

constexpr std::array<std::string_view, 18> keywords{"IF", "THEN", "ELSE",
    "END", "FOR", "IN", "DO", "NUMBER", "REQUIRE", "AND", "OR", "NOT",
    "TODAY", "GEQ", "GT", "LEQ", "LT", "EQ"};
constexpr std::array<std::string_view, 28> functions{"PAY", "LOGPAY", "NPV",
    "NPVMEM", "HISTFIXING", "DISCOUNT", "FWDCOMP", "FWDAVG", "ABOVEPROB",
    "BELOWPROB", "SIZE", "DATEINDEX", "SORT", "PERMUTE", "abs", "exp", "ln",
    "log", "sqrt", "pow", "max", "min", "normalCdf", "normalPdf", "black",
    "dcf", "days", "RESETNPVMEM"};

template <std::size_t N>
bool in(const std::array<std::string_view, N>& set, std::string_view s) {
    return std::ranges::find(set, s) != set.end();
}

/**
 * @brief Where the script raises: a source shaped as it does not expect.
 */
struct stopped : std::runtime_error {
    using std::runtime_error::runtime_error;
};

const pt::regex& ident() {
    static const pt::regex re(R"([A-Za-z_][A-Za-z0-9_]*)");
    return re;
}

bool is_ident(std::string_view s) {
    return ident().fullmatch(s).has_value();
}

using token_list = std::vector<std::string>;

token_list tokens(std::string_view code) {
    static const pt::regex re(R"([A-Za-z_][A-Za-z0-9_]*|\d+\.?\d*(?:[eE][-+]?)"
        R"(\d+)?|==|!=|<=|>=|[^\s])");
    token_list out;
    for (const auto& m : re.find_all(code))
        out.emplace_back(m.group());
    return out;
}

std::vector<std::string_view> split_on(std::string_view text, char by) {
    std::vector<std::string_view> out;
    for (std::size_t i = 0;;) {
        const auto j = text.find(by, i);
        out.push_back(text.substr(i, j == std::string_view::npos
            ? std::string_view::npos : j - i));
        if (j == std::string_view::npos)
            return out;
        i = j + 1;
    }
}

std::set<std::string> declared(std::string_view code) {
    static const pt::regex re(R"(\bNUMBER\b([^;]*);)");
    std::set<std::string> names;
    for (const auto& block : re.find_all(code))
        for (const auto part : split_on(block.group(1), ','))
            if (const auto m = ident().match(co::strip(part)))
                names.emplace(m->group());
    return names;
}

std::set<std::string> assigned(std::string_view code) {
    static const pt::regex re(R"((?:^|;|\bTHEN\b|\bELSE\b|\bDO\b)\s*)"
        R"(([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*=(?!=))");
    std::set<std::string> names;
    for (const auto& m : re.find_all(code))
        names.emplace(m.group(1));
    return names;
}

std::vector<token_list> call_arguments(const token_list& toks,
    std::size_t start) {
    std::vector<token_list> args;
    token_list current;
    int depth = 0;
    for (std::size_t i = start; i < toks.size(); ++i) {
        const auto& tok = toks[i];
        if (tok == "(") {
            ++depth;
            if (depth == 1)
                continue;
        } else if (tok == ")") {
            --depth;
            if (depth == 0) {
                args.push_back(std::move(current));
                return args;
            }
        } else if (tok == "," && depth == 1) {
            args.push_back(std::move(current));
            current.clear();
            continue;
        }
        current.push_back(tok);
    }
    args.push_back(std::move(current));
    return args;
}

std::optional<std::string> bare(const token_list& arg) {
    if (!arg.empty() && is_ident(arg[0]) && (arg.size() == 1 || arg[1] == "["))
        return arg[0];
    return std::nullopt;
}

struct input {
    std::string name;
    std::string type;
    bool array = false;
};

std::vector<input> infer(std::string_view code) {
    const auto toks = tokens(code);
    const auto local = declared(code);
    std::vector<std::string> order;
    std::map<std::string, std::optional<std::string>> kind;
    std::set<std::string> arrays;
    auto note = [&](const std::string& name,
        std::optional<std::string_view> type = std::nullopt) {
        if (local.contains(name) || in(keywords, name) || in(functions, name))
            return;
        auto [it, fresh] = kind.try_emplace(name);
        if (fresh)
            order.push_back(name);
        if (type && (!it->second || *it->second == "Number"))
            it->second = std::string(*type);
    };
    auto names_of = [&](std::size_t open) {
        std::vector<std::optional<std::string>> names;
        for (const auto& a : call_arguments(toks, open))
            names.push_back(bare(a));
        return names;
    };
    auto note_at = [&](const std::vector<std::optional<std::string>>& names,
        std::size_t pos, std::string_view type) {
        if (pos < names.size() && names[pos])
            note(*names[pos], type);
    };

    for (std::size_t i = 0; i < toks.size(); ++i) {
        const auto& tok = toks[i];
        if (!is_ident(tok) || in(keywords, tok))
            continue;
        const std::string next = i + 1 < toks.size() ? toks[i + 1] : "";
        if (in(functions, tok)) {
            if (next != "(")
                continue;
            const auto names = names_of(i + 1);
            if (tok == "PAY" || tok == "LOGPAY") {
                note_at(names, 1, "Event");
                note_at(names, 2, "Event");
                note_at(names, 3, "Currency");
            } else if (tok == "ABOVEPROB" || tok == "BELOWPROB") {
                note_at(names, 0, "Index");
                note_at(names, 1, "Event");
                note_at(names, 2, "Event");
            } else if (tok == "dcf" || tok == "days") {
                note_at(names, 0, "Daycounter");
                note_at(names, 1, "Event");
                note_at(names, 2, "Event");
            } else if (tok == "NPV") {
                note_at(names, 1, "Event");
            } else if (tok == "DATEINDEX") {
                note_at(names, 0, "Event");
                if (names.size() > 1 && names[1]) {
                    note(*names[1], "Event");
                    arrays.insert(*names[1]);
                }
            } else if (tok == "SIZE") {
                if (!names.empty() && names[0])
                    arrays.insert(*names[0]);
            } else if (tok == "FWDCOMP" || tok == "FWDAVG" ||
                tok == "HISTFIXING") {
                note_at(names, 0, "Index");
            }
            continue;
        }
        if (local.contains(tok))
            continue;
        note(tok);
        if (next == "[") {
            arrays.insert(tok);
            const auto close = std::find(toks.begin() +
                static_cast<std::ptrdiff_t>(i + 1), toks.end(), "]");
            if (close == toks.end())
                throw stopped("no closing bracket");
            const auto c = static_cast<std::size_t>(close - toks.begin());
            if (c + 1 < toks.size() && toks[c + 1] == "(") {
                note(tok, "Index");
                for (const auto& name : names_of(c + 1))
                    if (name)
                        note(*name, "Event");
            }
        } else if (next == "(") {
            note(tok, "Index");
            for (const auto& name : names_of(i + 1))
                if (name)
                    note(*name, "Event");
        }
    }
    std::vector<input> out;
    for (const auto& name : order)
        out.push_back({name, kind[name].value_or("Number"),
            arrays.contains(name)});
    return out;
}

/**
 * @brief str.split() joined by single blanks.
 */
std::string squeezed(std::string_view text) {
    std::string out;
    std::size_t i = 0;
    while (true) {
        i = co::skip_spaces(text, i);
        if (i >= text.size())
            return out;
        auto j = i;
        while (j < text.size() && !co::is_space(text[j]))
            ++j;
        (out += out.empty() ? "" : " ") += text.substr(i, j - i);
        i = j;
    }
}

using rules_map = std::map<std::string, std::string, std::less<>>;

void value_rules(std::string_view code, rules_map& rules) {
    static const pt::regex re(R"(\bREQUIRE\b([^;]*);)");
    for (const auto& m : re.find_all(code)) {
        const auto text = squeezed(m.group(1));
        if (const auto first = ident().search(text))
            rules.try_emplace(std::string(first->group()), text);
    }
}

struct comment_block {
    std::string title;
    rules_map described;
};

std::map<std::string, comment_block, std::less<>> comment_lines(
    std::string_view root_text) {
    static const pt::regex block(
        R"(<!--(.*?)-->\s*<Script>\s*<Name>([^<]+)</Name>)", {.dotall = true});
    static const pt::regex line_re(R"(\s*([A-Za-z_][A-Za-z0-9_]*)\s{2,}(.+))");
    std::map<std::string, comment_block, std::less<>> blocks;
    for (const auto& m : block.find_all(root_text)) {
        const auto lines = co::split_lines(co::strip(m.group(1)));
        comment_block b;
        if (!lines.empty())
            b.title = co::strip(lines[0]);
        for (std::size_t i = 1; i < lines.size(); ++i)
            if (const auto l = line_re.match(lines[i]))
                b.described.try_emplace(std::string(l->group(1)),
                    co::strip(l->group(2)));
        blocks[std::string(co::strip(m.group(2)))] = std::move(b);
    }
    return blocks;
}

struct variant_data {
    std::string purpose;
    std::string code;
    std::string npv;
    std::vector<std::pair<std::string, std::optional<std::string>>> results;
    std::vector<std::pair<std::string, std::vector<std::string>>> calibrations;
    std::vector<std::string> coarsenings;
};

struct script_input {
    input in;
    std::optional<std::string> rule;
    std::optional<std::string> description;
};

struct script {
    std::string name;
    std::optional<std::string> tag;
    std::optional<std::string> title;
    std::vector<variant_data> variants;
    std::vector<script_input> inputs;
};

std::string stripped_text(const pt::xml_element* e) {
    if (e == nullptr || !e->text)
        throw stopped("an element has no text");
    return std::string(co::strip(*e->text));
}

std::vector<script> load(std::string_view text) {
    const auto doc = pt::xml_document::parse(text);
    if (!doc)
        throw stopped("not XML");
    const auto comments = comment_lines(text);
    std::vector<script> scripts;
    for (const auto* s : doc->find_all(doc->root(), "Script")) {
        const auto name_text = doc->find_text(*s, "Name");
        if (!name_text)
            throw stopped("a script has no name");
        script out;
        out.name = co::strip(*name_text);
        const auto c = comments.find(out.name);
        const comment_block empty;
        const auto& comment = c == comments.end() ? empty : c->second;
        for (const auto* v : doc->find_all(*s, "Script")) {
            variant_data d;
            const auto purpose = pt::xml_document::get(*v, "purpose");
            d.purpose = purpose && !purpose->empty() ? std::string(*purpose)
                : "default";
            d.code = doc->find_text(*v, "Code").value_or("");
            d.npv = co::strip(doc->find_text(*v, "NPV").value_or(""));
            for (const auto* r : doc->find_all(*v, "Results/Result")) {
                const auto rename = pt::xml_document::get(*r, "rename");
                d.results.emplace_back(stripped_text(r), rename
                    ? std::optional<std::string>(*rename) : std::nullopt);
            }
            for (const auto* cal :
                     doc->find_all(*v, "CalibrationSpec/Calibration")) {
                std::vector<std::string> strikes;
                for (const auto* k : doc->find_all(*cal, "Strikes/Strike"))
                    strikes.push_back(stripped_text(k));
                const auto index = doc->find_text(*cal, "Index");
                if (!index)
                    throw stopped("a calibration has no index");
                d.calibrations.emplace_back(co::strip(*index), strikes);
            }
            for (const auto* e : doc->find_all(*v,
                     "ScheduleCoarsening/EligibleSchedule"))
                d.coarsenings.push_back(stripped_text(e));
            out.variants.push_back(std::move(d));
        }
        auto ordered = out.variants;
        std::ranges::stable_sort(ordered, {}, [](const variant_data& v) {
            return v.purpose != "default";
        });
        rules_map rules;
        std::set<std::string> seen;
        for (const auto& v : ordered) {
            value_rules(v.code, rules);
            const auto outputs = assigned(v.code);
            for (auto& i : infer(v.code))
                if (!seen.contains(i.name) && !outputs.contains(i.name)) {
                    seen.insert(i.name);
                    out.inputs.push_back({std::move(i), {}, {}});
                }
        }
        for (auto& i : out.inputs) {
            if (const auto r = rules.find(i.in.name); r != rules.end())
                i.rule = r->second;
            if (const auto d = comment.described.find(i.in.name);
                d != comment.described.end())
                i.description = d->second;
        }
        const auto tag_text = doc->find_text(*s, "ProductTag").value_or("");
        const auto tag = co::strip(tag_text);
        if (!tag.empty())
            out.tag = std::string(tag);
        if (!comment.title.empty())
            out.title = comment.title;
        scripts.push_back(std::move(out));
    }
    return scripts;
}

using sql_value = std::variant<std::monostate, bool, long long, std::string>;
using sql_row = std::vector<sql_value>;

sql_value text_or_null(const std::optional<std::string>& s) {
    return s ? sql_value(*s) : sql_value();
}

std::string literal(const sql_value& v) {
    if (std::holds_alternative<std::monostate>(v))
        return "null";
    if (const auto* b = std::get_if<bool>(&v))
        return *b ? "true" : "false";
    if (const auto* n = std::get_if<long long>(&v))
        return std::to_string(*n);
    const auto& s = std::get<std::string>(v);
    if (s.find('\n') != std::string::npos || s.find('\'') != std::string::npos) {
        std::string tag = "$code$";
        while (s.find(tag) != std::string::npos)
            tag.replace(tag.find('$'), 1, "$x");
        return tag + s + tag;
    }
    return "'" + s + "'";
}

std::string insert(std::string_view table,
    const std::vector<std::string_view>& columns,
    const std::vector<sql_row>& rows, std::string_view conflict) {
    if (rows.empty())
        return "";
    std::string body;
    for (std::size_t r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        body += r > 0 ? ",\n    (" : "    (";
        body += literal(row[0]) + ", ores_utility_system_tenant_id_fn(), 0, ";
        for (std::size_t i = 1; i < row.size(); ++i)
            body += (i > 1 ? ", " : "") + literal(row[i]);
        body += ", current_user, 'system.initial_load', 'Seed the script "
            "library')";
    }
    std::string head = std::string(columns[0]) + ", tenant_id, version, ";
    for (std::size_t i = 1; i < columns.size(); ++i)
        (head += i > 1 ? ", " : "") += columns[i];
    return "insert into " + std::string(table) + " (\n    " + head +
        ",\n    modified_by, change_reason_code, change_commentary\n) values\n" +
        body + "\non conflict (" + std::string(conflict) + ")\n"
        "where valid_to = ores_utility_infinity_timestamp_fn()\ndo nothing;\n\n";
}

std::string seed(const std::vector<script>& scripts) {
    std::vector<sql_row> s_rows, v_rows, r_rows, c_rows, k_rows, o_rows,
        i_rows;
    for (const auto& s : scripts) {
        const auto& n = s.name;
        s_rows.push_back({n, text_or_null(s.tag), text_or_null(s.title)});
        for (const auto& v : s.variants) {
            const auto& p = v.purpose;
            v_rows.push_back({n, p, v.code, v.npv});
            long long seq = 0;
            for (const auto& [var, rename] : v.results)
                r_rows.push_back({n, p, ++seq, var, text_or_null(rename)});
            long long cal = 0;
            for (const auto& [index, strikes] : v.calibrations) {
                c_rows.push_back({n, p, ++cal, index});
                long long k = 0;
                for (const auto& strike : strikes)
                    k_rows.push_back({n, p, cal, ++k, strike});
            }
            seq = 0;
            for (const auto& schedule : v.coarsenings)
                o_rows.push_back({n, p, ++seq, schedule});
        }
        long long seq = 0;
        for (const auto& i : s.inputs)
            i_rows.push_back({n, ++seq, i.in.name, i.in.type, i.in.array,
                text_or_null(i.rule), text_or_null(i.description)});
    }
    return "/* Generated by projects/ores.sql/utility/generate_script_library"
        ".py from ORE's\n * script library. Do not edit: run the script, "
        "which --check verifies. */\n\n\\echo '--- Script library ---'\n\n" +
        insert("ores_trading_scripts_tbl", {"name", "product_tag",
            "description"}, s_rows, "tenant_id, name") +
        insert("ores_trading_script_variants_tbl", {"script_name", "purpose",
            "code", "npv_variable"}, v_rows,
            "tenant_id, script_name, purpose") +
        insert("ores_trading_script_results_tbl", {"script_name", "purpose",
            "sequence_number", "variable", "rename"}, r_rows,
            "tenant_id, script_name, purpose, sequence_number") +
        insert("ores_trading_script_calibrations_tbl", {"script_name",
            "purpose", "sequence_number", "index_variable"}, c_rows,
            "tenant_id, script_name, purpose, sequence_number") +
        insert("ores_trading_script_calibration_strikes_tbl", {"script_name",
            "purpose", "calibration_number", "sequence_number",
            "strike_variable"}, k_rows, "tenant_id, script_name, purpose, "
            "calibration_number, sequence_number") +
        insert("ores_trading_script_coarsenings_tbl", {"script_name",
            "purpose", "sequence_number", "eligible_schedule"}, o_rows,
            "tenant_id, script_name, purpose, sequence_number") +
        insert("ores_trading_script_inputs_tbl", {"script_name",
            "sequence_number", "input_name", "data_type", "is_array",
            "value_rule", "description"}, i_rows,
            "tenant_id, script_name, sequence_number");
}

void library_seed(const ct::corpus_view& v, ct::file_output& out) {
    const auto fix = "Correct " + std::string(library_path) + ".";
    const auto text = v.read_file(library_path);
    if (!text) {
        co::refuse(out, seed_path, "cannot read " + std::string(library_path),
            fix);
        return;
    }
    try {
        out.files.add(seed_path, seed(load(*text)));
    } catch (const stopped& e) {
        co::refuse(out, seed_path, std::string(library_path) + ": " +
            e.what(), fix);
    }
}

}

const ct::descriptor script_library_seed{
    .name = "script_library_seed",
    .version = 1,
    .summary = "Writes the script library seed from ORE's script library.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = co::refusal_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{library_seed}};

}
