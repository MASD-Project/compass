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
#include "compass.docs/codecs/knowledge_titles.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <format>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.docs/ops/org_text.hpp"
#include "compass.org/ops/document_fields.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace cops = compass::codec::ops;
namespace oo = compass::org::ops;

constexpr std::string_view codec_name = "knowledge_titles";
constexpr std::string_view check_document =
    "compass/checks/knowledge_titles.org";

constexpr std::uint32_t sentence_title = 0;
constexpr std::uint32_t title_file_mismatch = 1;
constexpr std::uint32_t unreadable = 2;

constexpr std::array<ct::rule, 3> rules{{
    {"sentence_title", "A knowledge page's title is a noun phrase, like "
        "the title of a paper: it holds no word of the table \"Finite "
        "verbs\", and no word of the table \"Ambiguous\" when it opens with "
        "a word of the table \"Determiners\".", {}, ct::blame_method::line},
    {"title_file_mismatch", "A knowledge page's file name is its title in "
        "snake case; a structure note's ends in the hub suffix.", {},
        ct::blame_method::line},
    {"unreadable", "The check document gives every table, column and "
        "setting the codec reads, and its rows agree with each other."}}};

constexpr std::array<std::string_view, 1> verb_columns{"verb"};
constexpr std::array<std::string_view, 1> word_columns{"word"};
constexpr std::array<std::string_view, 2> skip_columns{"path", "reason"};
constexpr std::array<std::string_view, 2> setting_columns{"setting",
    "value"};

constexpr std::array<cops::table_need, 5> needed{{
    {"Finite verbs", verb_columns},
    {"Ambiguous", word_columns},
    {"Determiners", word_columns},
    {"Skip", skip_columns},
    {"Settings", setting_columns}}};

constexpr std::array<std::string_view, 4> setting_names{"skip_tag",
    "hub_suffix", "house_style", "title_file_mismatch"};

using word_set = std::set<std::string, std::less<>>;

/**
 * @brief Everything the check document states.
 */
struct project_data {
    word_set verbs;
    word_set ambiguous;
    word_set determiners;
    std::vector<std::string> skipped;
    std::string skip_tag = "structure_note";
    std::string hub_suffix = "_hub";
    std::string house_style;
    bool check_file_names = true;
};

char lowered(char c) {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool in_word(char c) {
    return (c >= 'a' && c <= 'z') || c == '\'' ||
        static_cast<unsigned char>(c) >= 0x80;
}

/**
 * @brief The words of @p title, lowercased: runs of letters and
 * apostrophes, without the apostrophes that open or close a run.
 */
std::vector<std::string> words_of(std::string_view title) {
    std::vector<std::string> out;
    std::string word;
    const auto flush = [&] {
        const auto begin = word.find_first_not_of('\'');
        if (begin != std::string::npos)
            out.push_back(word.substr(begin,
                word.find_last_not_of('\'') - begin + 1));
        word.clear();
    };
    for (const auto c : title) {
        const auto l = lowered(c);
        if (in_word(l))
            word.push_back(l);
        else
            flush();
    }
    flush();
    return out;
}

/**
 * @brief @p title in snake case: lowercased, each run of characters that
 * are not ASCII letters or digits one underscore, none at either end.
 */
std::string snake_case(std::string_view title) {
    std::string out;
    for (const auto c : title) {
        const auto l = lowered(c);
        if ((l >= 'a' && l <= 'z') || (l >= '0' && l <= '9'))
            out.push_back(l);
        else if (!out.empty() && out.back() != '_')
            out.push_back('_');
    }
    if (!out.empty() && out.back() == '_')
        out.pop_back();
    return out;
}

std::string lowercase(std::string_view s) {
    std::string out;
    for (const auto c : s)
        out.push_back(lowered(c));
    return out;
}

/**
 * @brief What the check document states, or why it states it wrongly,
 * written for the reader of the finding.
 */
std::expected<project_data, std::string> read_document(
    const cops::data_tables& data) {
    const auto wrong = [&](const cops::data_table& t, std::uint32_t line,
                           const std::string& what) {
        return std::unexpected(std::format("{}:{}: {}, in the table \"{}\" "
            "{} reads", data.source, line, what, t.name, codec_name));
    };
    project_data out;

    const auto words = [&](std::string_view name, std::string_view column,
                           word_set& into) -> std::expected<void, std::string> {
        const auto& t = *data.find(name);
        for (std::size_t r = 0; r < t.rows.size(); ++r) {
            const auto word = lowercase(t.rows[r][t.column(column)]);
            if (word.empty() || words_of(word) != std::vector{word})
                return wrong(t, t.row_lines[r], "the row names no single "
                    "word");
            into.insert(word);
        }
        return {};
    };
    if (auto r = words("Finite verbs", "verb", out.verbs); !r)
        return std::unexpected(r.error());
    if (auto r = words("Ambiguous", "word", out.ambiguous); !r)
        return std::unexpected(r.error());
    if (auto r = words("Determiners", "word", out.determiners); !r)
        return std::unexpected(r.error());
    for (const auto& word : out.ambiguous)
        if (out.verbs.contains(word))
            return wrong(*data.find("Ambiguous"), data.find("Ambiguous")->line,
                "the word '" + word + "' is also in the table \"Finite "
                "verbs\", which counts it whatever opens the title");

    const auto& k = *data.find("Skip");
    for (std::size_t r = 0; r < k.rows.size(); ++r) {
        const auto& path = k.rows[r][k.column("path")];
        if (path.empty())
            return wrong(k, k.row_lines[r], "the row names no path");
        out.skipped.push_back(path);
    }

    const auto& s = *data.find("Settings");
    std::set<std::string, std::less<>> seen;
    for (std::size_t r = 0; r < s.rows.size(); ++r) {
        const auto& name = s.rows[r][s.column("setting")];
        const auto& value = s.rows[r][s.column("value")];
        if (std::ranges::find(setting_names, name) == setting_names.end())
            return wrong(s, s.row_lines[r], "the setting '" + name +
                "' is not one " + std::string(codec_name) + " has");
        if (!seen.insert(name).second)
            return wrong(s, s.row_lines[r], "the setting '" + name +
                "' is stated twice");
        if (name == "house_style")
            out.house_style = value;
        else if (value.empty())
            return wrong(s, s.row_lines[r], "the setting '" + name +
                "' has no value");
        else if (name == "skip_tag")
            out.skip_tag = value;
        else if (name == "hub_suffix")
            out.hub_suffix = value;
        else if (value == "yes" || value == "no")
            out.check_file_names = value == "yes";
        else
            return wrong(s, s.row_lines[r], "the setting "
                "'title_file_mismatch' is '" + value + "', not yes or no");
    }
    return out;
}

/**
 * @brief The words of @p words that make the title a sentence, each once,
 * in the order they come.
 */
std::vector<std::string> finite_verbs(const project_data& d,
    const std::vector<std::string>& words) {
    const bool determined = !words.empty() &&
        d.determiners.contains(words.front());
    std::vector<std::string> out;
    for (const auto& w : words) {
        const bool verb = d.verbs.contains(w) ||
            (determined && d.ambiguous.contains(w));
        if (verb && std::ranges::find(out, w) == out.end())
            out.push_back(w);
    }
    return out;
}

void add(ct::diagnostic_table& out, const ct::document& doc,
    std::uint32_t offset, std::uint32_t rule, std::string_view message,
    std::string_view fix) {
    ct::diagnostic d;
    d.document = doc.row;
    d.range = {offset, offset};
    d.rule = ct::rule_id{rule};
    d.message = message;
    d.fix = fix;
    out.add(d);
}

void check(const project_data& d, const ct::document& doc,
    ct::diagnostic_table& out) {
    if (ops::keyword_value(doc, "type") != "knowledge")
        return;
    if (std::ranges::any_of(d.skipped, [&](const std::string& p) {
            return doc.path.starts_with(p);
        }))
        return;
    const auto k = oo::find_keyword(doc.tables, "title");
    if (k.is_none())
        return;
    const auto title = ops::keyword_value(doc, "title");
    const auto at = doc.tables.elements.ranges[
        doc.tables.keywords.elements[k]].begin;
    const bool structure = ops::has_tag(ops::keyword_value(doc, "filetags"),
        d.skip_tag);

    if (!structure) {
        const auto verbs = finite_verbs(d, words_of(title));
        if (!verbs.empty()) {
            std::string listed;
            for (const auto& v : verbs)
                listed += (listed.empty() ? "'" : ", '") + v + "'";
            add(out, doc, at, sentence_title, std::format("the title \"{}\" "
                "is a sentence: it holds the finite verb {}", title, listed),
                "Retitle the page as a noun phrase, like the title of a "
                "paper, and state its claim in the first sentence of its "
                "summary." + (d.house_style.empty() ? std::string()
                    : " See " + d.house_style + "."));
        }
    }

    if (!d.check_file_names)
        return;
    auto stem = doc.path.substr(doc.path.rfind('/') + 1);
    if (stem.ends_with(".org"))
        stem.remove_suffix(4);
    if (structure) {
        if (!stem.ends_with(d.hub_suffix))
            add(out, doc, at, title_file_mismatch, std::format("the page is "
                "a structure note, and its file name {} does not end in {}",
                stem, d.hub_suffix), std::format("Rename the file so its "
                "name ends in {}.", d.hub_suffix));
        return;
    }
    const auto expected = snake_case(title);
    if (stem != expected)
        add(out, doc, at, title_file_mismatch, std::format("the file name {} "
            "is not the title in snake case, {}", stem, expected),
            std::format("Rename the file {}.org, or retitle the page.",
                expected));
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto data = cops::tables_for(codec_name, v.option(cops::data_option),
        needed);
    if (!data) {
        co::report(out, check_document, 0, unreadable, data.error(),
            "Give the tables the message names.");
        return;
    }
    const auto document = read_document(*data);
    if (!document) {
        co::report(out, data->source, 0, unreadable, document.error(),
            "Correct the row the message names.");
        return;
    }
    for (std::size_t i = 0; i < v.size(); ++i)
        check(*document, v[i], out);
}

constexpr std::array<ct::option, 1> options{{
    {cops::data_option, "", "The tables of the check document whose "
        "Definition names this codec: the finite verbs, the words that are "
        "verbs only after a determiner, the determiners, the paths to skip, "
        "and the settings. compass compiles them; not a setting."}}};

}

const ct::descriptor knowledge_titles{
    .name = "knowledge_titles",
    .version = 1,
    .summary = "Reports each knowledge page whose title is a sentence, and "
               "each whose file name is not its title in snake case.",
    .reads = ct::inputs::every_document(),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
