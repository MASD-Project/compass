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
#include "compass.sql/ops/hygiene.hpp"

#include <array>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace compass::sql::ops {

namespace {

constexpr auto npos = std::string_view::npos;

constexpr bool is_space(char c) noexcept {
    return c == ' ' || (c >= '\t' && c <= '\r') || (c >= '\x1c' &&
        c <= '\x1f');
}

constexpr bool is_word(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' ||
        static_cast<unsigned char>(c) >= 0x80;
}

constexpr bool is_name(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_';
}

constexpr bool is_quoted_name(char c) noexcept {
    return is_name(c) || c == '"';
}

constexpr char lower(char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string lowered(std::string_view text) {
    std::string out(text);
    for (auto& c : out)
        c = lower(c);
    return out;
}

std::size_t skip_spaces(std::string_view text, std::size_t i) noexcept {
    while (i < text.size() && is_space(text[i]))
        ++i;
    return i;
}

/**
 * @brief The end of @p word at @p i, compared without case, or npos.
 */
std::size_t keyword(std::string_view text, std::size_t i,
    std::string_view word) noexcept {
    if (i == npos || i + word.size() > text.size())
        return npos;
    for (std::size_t k = 0; k < word.size(); ++k) {
        if (lower(text[i + k]) != word[k])
            return npos;
    }
    return i + word.size();
}

/**
 * @brief The end of the spaces at @p i when there is at least one, or
 * npos: \s+.
 */
std::size_t spaces(std::string_view text, std::size_t i) noexcept {
    if (i == npos)
        return npos;
    const auto end = skip_spaces(text, i);
    return end == i ? npos : end;
}

/**
 * @brief The end of the words of @p words at @p i, each followed by at
 * least one space, or npos: word\s+word\s+...
 */
std::size_t words(std::string_view text, std::size_t i,
    std::initializer_list<std::string_view> words) noexcept {
    for (const auto w : words)
        i = spaces(text, keyword(text, i, w));
    return i;
}

/**
 * @brief The end of the longest run of @p accept at @p i, or npos when
 * there is none.
 */
template <typename Accept>
std::size_t run(std::string_view text, std::size_t i, Accept accept) {
    if (i == npos)
        return npos;
    auto end = i;
    while (end < text.size() && accept(text[end]))
        ++end;
    return end == i ? npos : end;
}

bool word_starts(std::string_view text, std::size_t i) noexcept {
    return i == 0 || !is_word(text[i - 1]);
}

/**
 * @brief Each position of @p text where @p word starts, without case,
 * after a word boundary.
 */
std::vector<std::size_t> starts_of(std::string_view text,
    std::string_view word) {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i + word.size() <= text.size(); ++i) {
        if (lower(text[i]) == word[0] && keyword(text, i, word) != npos &&
            word_starts(text, i))
            out.push_back(i);
    }
    return out;
}

/**
 * @brief The dollar-quote tag at @p i, $[A-Za-z_0-9]*$, or empty.
 */
std::string_view dollar_tag(std::string_view text, std::size_t i) noexcept {
    if (i >= text.size() || text[i] != '$')
        return {};
    auto j = i + 1;
    while (j < text.size() && is_name(text[j]))
        ++j;
    if (j >= text.size() || text[j] != '$')
        return {};
    return text.substr(i, j + 1 - i);
}

std::size_t after_quoted(std::string_view text, std::size_t i, char quote) {
    const auto n = text.size();
    auto j = i + 1;
    while (j < n) {
        if (text[j] == quote) {
            if (j + 1 < n && text[j + 1] == quote) {
                j += 2;
                continue;
            }
            return j + 1;
        }
        ++j;
    }
    return n;
}

std::size_t after_block_comment(std::string_view text, std::size_t i) {
    const auto n = text.size();
    int depth = 1;
    auto j = i + 2;
    while (j < n) {
        if (text.substr(j, 2) == "/*") {
            ++depth;
            j += 2;
            continue;
        }
        if (text.substr(j, 2) == "*/") {
            --depth;
            j += 2;
            if (depth == 0)
                return j;
            continue;
        }
        ++j;
    }
    return n;
}

void split_into(std::string_view text, std::uint32_t base,
    std::vector<std::uint32_t>& out) {
    const auto n = text.size();
    std::size_t i = 0;
    while (i < n) {
        if (text.substr(i, 2) == "--") {
            const auto end = text.find('\n', i);
            i = end == npos ? n : end;
            continue;
        }
        if (text.substr(i, 2) == "/*") {
            i = after_block_comment(text, i);
            continue;
        }
        if (const auto tag = dollar_tag(text, i); !tag.empty()) {
            const auto body = i + tag.size();
            const auto end = text.find(tag, body);
            const auto body_end = end == npos ? n : end;
            split_into(text.substr(body, body_end - body),
                base + static_cast<std::uint32_t>(body), out);
            i = end == npos ? n : end + tag.size();
            continue;
        }
        if (text[i] == '"') {
            i = after_quoted(text, i, '"');
            continue;
        }
        if (text[i] != '\'') {
            ++i;
            continue;
        }
        i = after_quoted(text, i, '\'');
        auto j = i;
        auto comment_at = npos;
        while (j < n) {
            const auto c = text[j];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++j;
                continue;
            }
            if (text.substr(j, 2) == "--") {
                comment_at = comment_at == npos ? j : comment_at;
                const auto end = text.find('\n', j);
                j = end == npos ? n : end + 1;
                continue;
            }
            if (text.substr(j, 2) == "/*") {
                comment_at = comment_at == npos ? j : comment_at;
                j = after_block_comment(text, j);
                continue;
            }
            break;
        }
        if (comment_at != npos && j < n && text[j] == '\'')
            out.push_back(base + static_cast<std::uint32_t>(comment_at));
    }
}

/**
 * @brief The end of
 * create\s+(unique\s+)?index\s+if\s+not\s+exists\s+NAME\s*\n?\s*on\s+"?TABLE"?\s*\(COLUMNS\)
 * at @p i, with its three groups, or npos. NAME gives back characters
 * from its end until the rest matches, as the regular expression does.
 */
std::size_t index_at(std::string_view text, std::size_t i,
    std::string_view& name, std::string_view& table,
    std::string_view& columns) {
    auto j = spaces(text, keyword(text, i, "create"));
    if (const auto unique = words(text, j, {"unique"}); unique != npos)
        j = unique;
    j = words(text, j, {"index", "if", "not", "exists"});
    const auto name_end = run(text, j, is_name);
    if (name_end == npos)
        return npos;
    for (auto end = name_end; end > j; --end) {
        auto k = spaces(text, keyword(text, skip_spaces(text, end), "on"));
        if (k == npos)
            continue;
        if (k < text.size() && text[k] == '"')
            ++k;
        const auto table_end = run(text, k, is_name);
        if (table_end == npos)
            continue;
        auto p = table_end;
        if (p < text.size() && text[p] == '"')
            ++p;
        p = skip_spaces(text, p);
        if (p >= text.size() || text[p] != '(')
            continue;
        const auto close = text.find(')', p + 1);
        if (close == npos)
            continue;
        name = text.substr(j, end - j);
        table = text.substr(k, table_end - k);
        columns = text.substr(p + 1, close - p - 1);
        return close + 1;
    }
    return npos;
}

std::string collapsed_lower(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        if (is_space(text[i])) {
            ++i;
            continue;
        }
        if (!out.empty())
            out.push_back(' ');
        while (i < text.size() && !is_space(text[i]))
            out.push_back(lower(text[i++]));
    }
    return out;
}

std::string bare(std::string_view name) {
    auto out = lowered(name);
    const auto first = out.find_first_not_of('"');
    if (first == std::string::npos)
        return {};
    const auto last = out.find_last_not_of('"');
    return out.substr(first, last - first + 1);
}

/**
 * @brief The DDL that must say "if not exists" and the DDL that must say
 * "or replace", with the message of each.
 */
struct simple_rule {
    std::initializer_list<std::string_view> words;
    bool if_not_exists;
    std::string_view why;
};

const std::array<simple_rule, 10>& simple_rules() {
    static const std::array<simple_rule, 10> rules{{
        {{"table"}, true, "create table without `if not exists`"},
        {{"sequence"}, true, "create sequence without `if not exists`"},
        {{"extension"}, true, "create extension without `if not exists`"},
        {{"schema"}, true, "create schema without `if not exists`"},
        {{"materialized", "view"}, true,
            "create materialized view without `if not exists`"},
        {{"trigger"}, false, "create trigger without `or replace`"},
        {{"rule"}, false, "create rule without `or replace`"},
        {{"view"}, false, "create view without `or replace`"},
        {{"function"}, false, "create function without `or replace`"},
        {{"procedure"}, false, "create procedure without `or replace`"}}};
    return rules;
}

/**
 * @brief Whether \bcreate\s+WORDS\s+(?!if\s+not\s+exists) matches at
 * @p i. The last \s+ gives back spaces, so a run of two or more always
 * leaves a space where the lookahead looks.
 */
bool simple_matches(std::string_view text, std::size_t i,
    const simple_rule& rule) {
    auto j = spaces(text, keyword(text, i, "create"));
    std::size_t before_last = npos;
    for (const auto w : rule.words) {
        before_last = keyword(text, j, w);
        j = spaces(text, before_last);
    }
    if (j == npos)
        return false;
    if (!rule.if_not_exists)
        return true;
    return j - before_last >= 2 ||
        words(text, j, {"if", "not"}) == npos ||
        keyword(text, words(text, j, {"if", "not"}), "exists") == npos;
}

/**
 * @brief NAME\s+on\s+TABLE at @p i, names of [a-z0-9_"]+, or nothing.
 */
bool name_on_table(std::string_view text, std::size_t i,
    std::string_view& name, std::string_view& table) {
    const auto name_end = run(text, i, is_quoted_name);
    const auto t = words(text, spaces(text, name_end), {"on"});
    const auto table_end = run(text, t, is_quoted_name);
    if (table_end == npos)
        return false;
    name = text.substr(i, name_end - i);
    table = text.substr(t, table_end - t);
    return true;
}

}

open_comment unterminated_comment(std::string_view text) {
    const auto n = text.size();
    open_comment r;
    std::size_t i = 0;
    while (i < n) {
        if (r.depth > 0) {
            if (text.substr(i, 2) == "/*") {
                ++r.depth;
                i += 2;
            } else if (text.substr(i, 2) == "*/") {
                --r.depth;
                i += 2;
            } else {
                ++i;
            }
            continue;
        }
        if (text.substr(i, 2) == "--") {
            const auto end = text.find('\n', i);
            i = end == npos ? n : end;
            continue;
        }
        if (text.substr(i, 2) == "/*") {
            r.depth = 1;
            r.offset = static_cast<std::uint32_t>(i);
            i += 2;
            continue;
        }
        if (const auto tag = dollar_tag(text, i); !tag.empty()) {
            const auto end = text.find(tag, i + tag.size());
            i = end == npos ? n : end + tag.size();
            continue;
        }
        const auto c = text[i];
        if (c == '\'' || c == '"') {
            ++i;
            while (i < n) {
                if (text[i] == c) {
                    if (c == '\'' && text.substr(i, 2) == "''") {
                        i += 2;
                        continue;
                    }
                    break;
                }
                ++i;
            }
            ++i;
            continue;
        }
        ++i;
    }
    if (r.depth == 0)
        r.offset = 0;
    return r;
}

std::vector<duplicate_index> duplicate_indexes(std::string_view text) {
    std::vector<duplicate_index> seen;
    std::map<std::tuple<std::string, std::string, std::string>, std::size_t>
        rows;
    for (std::size_t i = 0; i < text.size();) {
        std::string_view name, table, columns;
        const auto end = lower(text[i]) == 'c'
            ? index_at(text, i, name, table, columns) : npos;
        if (end == npos) {
            ++i;
            continue;
        }
        auto key = std::make_tuple(lowered(name), lowered(table),
            collapsed_lower(columns));
        const auto [it, added] = rows.try_emplace(key, seen.size());
        if (added)
            seen.push_back({std::get<0>(key), std::get<1>(key),
                std::get<2>(key), 0, static_cast<std::uint32_t>(i)});
        ++seen[it->second].count;
        i = end;
    }
    std::erase_if(seen, [](const duplicate_index& d) { return d.count < 2; });
    return seen;
}

std::vector<std::uint32_t> split_concatenations(std::string_view text) {
    std::vector<std::uint32_t> out;
    split_into(text, 0, out);
    return out;
}

std::string mask_non_code(std::string_view text) {
    std::string out(text);
    const auto n = text.size();
    const auto blank = [&](std::size_t from, std::size_t to) {
        for (auto k = from; k < to; ++k) {
            if (out[k] != '\n')
                out[k] = ' ';
        }
    };
    std::size_t i = 0;
    int depth = 0;
    while (i < n) {
        if (depth > 0) {
            if (text.substr(i, 2) == "/*") {
                out[i] = out[i + 1] = ' ';
                ++depth;
                i += 2;
            } else if (text.substr(i, 2) == "*/") {
                out[i] = out[i + 1] = ' ';
                --depth;
                i += 2;
            } else {
                blank(i, i + 1);
                ++i;
            }
            continue;
        }
        if (text.substr(i, 2) == "--") {
            auto end = text.find('\n', i);
            end = end == npos ? n : end;
            blank(i, end);
            i = end;
            continue;
        }
        if (text.substr(i, 2) == "/*") {
            out[i] = out[i + 1] = ' ';
            ++depth;
            i += 2;
            continue;
        }
        if (const auto tag = dollar_tag(text, i); !tag.empty()) {
            auto end = text.find(tag, i + tag.size());
            end = end == npos ? n : end + tag.size();
            blank(i, end);
            i = end;
            continue;
        }
        const auto c = text[i];
        if (c == '"') {
            auto j = i + 1;
            while (j < n) {
                if (text[j] == '"') {
                    if (text.substr(j, 2) == "\"\"") {
                        j += 2;
                        continue;
                    }
                    ++j;
                    break;
                }
                ++j;
            }
            i = j;
            continue;
        }
        if (c == '\'') {
            auto j = i + 1;
            while (j < n) {
                if (text[j] == '\'') {
                    if (text.substr(j, 2) == "''") {
                        j += 2;
                        continue;
                    }
                    ++j;
                    break;
                }
                ++j;
            }
            blank(i, std::min(j, n));
            i = j;
            continue;
        }
        ++i;
    }
    return out;
}

std::vector<not_re_runnable> not_re_runnables(std::string_view text) {
    const auto masked = mask_non_code(text);
    const std::string_view m(masked);
    std::vector<not_re_runnable> out;
    const auto creates = starts_of(m, "create");
    const auto drops = starts_of(m, "drop");

    for (const auto& rule : simple_rules()) {
        for (const auto at : creates) {
            if (simple_matches(m, at, rule))
                out.push_back({static_cast<std::uint32_t>(at),
                    std::string(rule.why)});
        }
    }

    for (const auto at : creates) {
        auto j = spaces(m, keyword(m, at, "create"));
        if (const auto unique = words(m, j, {"unique"}); unique != npos)
            j = unique;
        j = words(m, j, {"index"});
        if (j == npos)
            continue;
        const auto tail = lowered(m.substr(j, 48));
        if (!tail.starts_with("if not exists") &&
            !tail.starts_with("concurrently if not exists"))
            out.push_back({static_cast<std::uint32_t>(at),
                "create index without `if not exists`"});
    }

    std::set<std::pair<std::string, std::string>> dropped_policies;
    std::set<std::pair<std::string, std::string>> dropped_types;
    for (const auto at : drops) {
        const auto after = spaces(m, keyword(m, at, "drop"));
        std::string_view name, table;
        const auto policy = words(m, after, {"policy", "if", "exists"});
        if (policy != npos && name_on_table(m, policy, name, table))
            dropped_policies.emplace(bare(name), bare(table));
        for (const auto kind : {std::string_view("type"),
                 std::string_view("domain")}) {
            const auto j = words(m, after, {kind, "if", "exists"});
            const auto end = run(m, j, is_quoted_name);
            if (end != npos)
                dropped_types.emplace(std::string(kind),
                    bare(m.substr(j, end - j)));
        }
    }

    for (const auto at : creates) {
        const auto j = words(m, spaces(m, keyword(m, at, "create")),
            {"policy"});
        std::string_view name, table;
        if (j == npos || !name_on_table(m, j, name, table))
            continue;
        if (!dropped_policies.contains({bare(name), bare(table)}))
            out.push_back({static_cast<std::uint32_t>(at),
                "create policy " + std::string(name) +
                    " with no `drop policy if exists`"});
    }

    for (const auto at : creates) {
        const auto after = spaces(m, keyword(m, at, "create"));
        for (const auto kind : {std::string_view("type"),
                 std::string_view("domain")}) {
            const auto j = words(m, after, {kind});
            const auto end = run(m, j, is_quoted_name);
            if (end == npos)
                continue;
            const auto name = m.substr(j, end - j);
            if (!dropped_types.contains({std::string(kind), bare(name)}))
                out.push_back({static_cast<std::uint32_t>(at),
                    "create " + std::string(m.substr(after, kind.size())) +
                        " " + std::string(name) +
                        " with no `drop ... if exists`"});
            break;
        }
    }
    return out;
}

}
