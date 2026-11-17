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

bool is_comment(const token& t) noexcept {
    return t.kind == token_kind::line_comment ||
        t.kind == token_kind::block_comment;
}

/**
 * @brief Whether the bytes from @p from to @p to are spaces, tabs and line
 * ends only, the white space PostgreSQL joins two literals across.
 */
bool joining_space(std::string_view text, std::uint32_t from,
    std::uint32_t to) noexcept {
    for (auto i = from; i < to; ++i) {
        const auto c = text[i];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            return false;
    }
    return true;
}

void split_into(std::string_view text, const std::vector<token>& tokens,
    std::vector<std::uint32_t>& out) {
    for (std::size_t k = 0; k < tokens.size(); ++k) {
        const auto& t = tokens[k];
        if (t.kind == token_kind::dollar_body) {
            const auto content = dollar_content(text_of(text, t));
            const auto base = static_cast<std::uint32_t>(
                content.data() - text.data());
            split_into(text, lex(content, base, false).tokens, out);
            continue;
        }
        if (t.kind != token_kind::string)
            continue;
        auto end = t.offset + t.size;
        auto j = k + 1;
        while (j < tokens.size() && is_comment(tokens[j]) &&
            joining_space(text, end, tokens[j].offset)) {
            end = tokens[j].offset + tokens[j].size;
            ++j;
        }
        if (j > k + 1 && j < tokens.size() &&
            tokens[j].kind == token_kind::string &&
            joining_space(text, end, tokens[j].offset))
            out.push_back(tokens[k + 1].offset);
    }
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
 * @brief The top-level code tokens of a file, with readers over them.
 */
struct code_tokens {
    std::string_view text;
    std::vector<token> code;

    bool is(std::size_t i, std::string_view lower_word) const noexcept {
        return i < code.size() && code[i].kind == token_kind::word &&
            is_keyword(text_of(text, code[i]), lower_word);
    }

    bool names(std::size_t i) const noexcept {
        return i < code.size() && (code[i].kind == token_kind::word ||
            code[i].kind == token_kind::quoted_name);
    }

    bool if_not_exists(std::size_t i) const noexcept {
        return is(i, "if") && is(i + 1, "not") && is(i + 2, "exists");
    }

    std::string bare(std::size_t i) const {
        const auto bytes = text_of(text, code[i]);
        return code[i].kind == token_kind::quoted_name
            ? lowered(string_value(bytes)) : lowered(bytes);
    }

    /**
     * @brief The bare name at @p i, the schema dropped, and the index
     * after it.
     */
    std::pair<std::string, std::size_t> qualified(std::size_t i) const {
        auto name = bare(i);
        ++i;
        while (i + 1 < code.size() && code[i].kind == token_kind::symbol &&
            text[code[i].offset] == '.' && names(i + 1)) {
            name = bare(i + 1);
            i += 2;
        }
        return {std::move(name), i};
    }
};

}

std::vector<duplicate_index> duplicate_indexes(const sql_file& model) {
    std::vector<duplicate_index> seen;
    std::map<std::tuple<std::string, std::string, std::string>, std::size_t>
        rows;
    for (const auto& x : model.indexes) {
        if (!x.if_not_exists || x.name.empty())
            continue;
        const auto [it, added] = rows.try_emplace(
            std::make_tuple(x.name, x.table, x.columns), seen.size());
        if (added)
            seen.push_back({x.name, x.table, x.columns, 0, x.offset});
        ++seen[it->second].count;
    }
    std::erase_if(seen, [](const duplicate_index& d) { return d.count < 2; });
    return seen;
}

std::vector<std::uint32_t> split_concatenations(std::string_view text,
    const lexed& tokens) {
    std::vector<std::uint32_t> out;
    split_into(text, tokens.tokens, out);
    return out;
}

std::vector<not_re_runnable> not_re_runnables(std::string_view text,
    const lexed& tokens) {
    code_tokens c{text, {}};
    c.code.reserve(tokens.tokens.size());
    for (const auto& t : tokens.tokens) {
        if (!is_comment(t) && t.kind != token_kind::meta)
            c.code.push_back(t);
    }
    constexpr std::array<std::string_view, 2> kinds{"type", "domain"};

    std::set<std::pair<std::string, std::string>> dropped_policies;
    std::set<std::pair<std::string_view, std::string>> dropped_types;
    for (std::size_t i = 0; i < c.code.size(); ++i) {
        if (!c.is(i, "drop"))
            continue;
        if (c.is(i + 1, "policy") && c.is(i + 2, "if") &&
            c.is(i + 3, "exists") && c.names(i + 4) && c.is(i + 5, "on") &&
            c.names(i + 6))
            dropped_policies.emplace(c.bare(i + 4), c.qualified(i + 6).first);
        for (const auto kind : kinds) {
            if (c.is(i + 1, kind) && c.is(i + 2, "if") &&
                c.is(i + 3, "exists") && c.names(i + 4))
                dropped_types.emplace(kind, c.qualified(i + 4).first);
        }
    }

    std::vector<not_re_runnable> out;
    const auto report = [&](std::size_t i, std::string why) {
        out.push_back({c.code[i].offset, std::move(why)});
    };
    for (std::size_t i = 0; i < c.code.size(); ++i) {
        if (!c.is(i, "create"))
            continue;
        const auto j = i + 1;
        for (const auto& rule : simple_rules()) {
            auto k = j;
            bool matched = true;
            for (const auto w : rule.words)
                matched = matched && c.is(k++, w);
            if (matched && (!rule.if_not_exists || !c.if_not_exists(k)))
                report(i, std::string(rule.why));
        }
        const auto index = c.is(j, "unique") ? j + 1 : j;
        if (c.is(index, "index") && !c.if_not_exists(index + 1) &&
            !(c.is(index + 1, "concurrently") && c.if_not_exists(index + 2)))
            report(i, "create index without `if not exists`");
        if (c.is(j, "policy") && c.names(j + 1) && c.is(j + 2, "on") &&
            c.names(j + 3) && !dropped_policies.contains(
                {c.bare(j + 1), c.qualified(j + 3).first}))
            report(i, "create policy " +
                std::string(text_of(text, c.code[j + 1])) +
                " with no `drop policy if exists`");
        for (const auto kind : kinds) {
            if (!c.is(j, kind) || !c.names(j + 1))
                continue;
            const auto [name, after] = c.qualified(j + 1);
            if (!dropped_types.contains({kind, name}))
                report(i, "create " + std::string(text_of(text, c.code[j])) +
                    " " + std::string(text_of(text, c.code[after - 1])) +
                    " with no `drop ... if exists`");
        }
    }
    return out;
}

}
