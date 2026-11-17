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
#include "compass.sql/ops/sql_model.hpp"

#include <algorithm>
#include <array>
#include <span>

namespace compass::sql::ops {

namespace {

constexpr auto npos = std::string_view::npos;

using tokens_t = std::span<const token>;

constexpr bool is_space(char c) noexcept {
    return c == ' ' || (c >= '\t' && c <= '\r') || (c >= '\x1c' &&
        c <= '\x1f');
}

bool is_comment(const token& t) noexcept {
    return t.kind == token_kind::line_comment ||
        t.kind == token_kind::block_comment;
}

/**
 * @brief The lower-cased name a word or quoted name token states.
 */
std::string name_of(std::string_view bytes, token_kind kind) {
    if (kind == token_kind::quoted_name)
        return lowered(string_value(bytes));
    return lowered(bytes);
}

/**
 * @brief The text from the first byte of @p first to the last of @p last,
 * with its runs of spaces collapsed to one and lower-cased.
 */
std::string collapsed(std::string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size();) {
        if (is_space(text[i])) {
            ++i;
            continue;
        }
        if (!out.empty())
            out.push_back(' ');
        while (i < text.size() && !is_space(text[i]))
            out.push_back(text[i++]);
    }
    return lowered(out);
}

struct walker {
    std::string_view text;
    sql_file& out;

    std::string_view of(const token& t) const noexcept {
        return text.substr(t.offset, t.size);
    }

    bool is(const token& t, std::string_view lower_word) const noexcept {
        return t.kind == token_kind::word && is_keyword(of(t), lower_word);
    }

    bool is(tokens_t s, std::size_t i, std::string_view lower_word) const
        noexcept {
        return i < s.size() && is(s[i], lower_word);
    }

    bool symbol(tokens_t s, std::size_t i, char c) const noexcept {
        return i < s.size() && s[i].kind == token_kind::symbol &&
            text[s[i].offset] == c;
    }

    bool names(tokens_t s, std::size_t i) const noexcept {
        return i < s.size() && (s[i].kind == token_kind::word ||
            s[i].kind == token_kind::quoted_name);
    }

    /**
     * @brief The name at @p i, schema dropped, and the index after it, or
     * npos.
     */
    std::pair<std::string, std::size_t> qualified(tokens_t s,
        std::size_t i) const {
        if (!names(s, i))
            return {{}, npos};
        auto name = name_of(of(s[i]), s[i].kind);
        ++i;
        while (symbol(s, i, '.') && names(s, i + 1)) {
            name = name_of(of(s[i + 1]), s[i + 1].kind);
            i += 2;
        }
        return {std::move(name), i};
    }

    /**
     * @brief The index of the parenthesis that closes the one at @p open,
     * or npos.
     */
    std::size_t closing(tokens_t s, std::size_t open) const noexcept {
        int depth = 0;
        for (auto i = open; i < s.size(); ++i) {
            if (s[i].kind != token_kind::symbol)
                continue;
            const auto c = text[s[i].offset];
            if (c == '(' || c == '[') {
                ++depth;
            } else if (c == ')' || c == ']') {
                if (--depth == 0)
                    return i;
            }
        }
        return npos;
    }

    /**
     * @brief The top-level elements between @p open and @p close, split at
     * commas.
     */
    std::vector<tokens_t> elements(tokens_t s, std::size_t open,
        std::size_t close) const {
        std::vector<tokens_t> out;
        int depth = 0;
        auto start = open + 1;
        for (auto i = open + 1; i < close; ++i) {
            if (s[i].kind != token_kind::symbol)
                continue;
            const auto c = text[s[i].offset];
            if (c == '(' || c == '[')
                ++depth;
            else if (c == ')' || c == ']')
                --depth;
            else if (c == ',' && depth == 0) {
                out.push_back(s.subspan(start, i - start));
                start = i + 1;
            }
        }
        out.push_back(s.subspan(start, close - start));
        return out;
    }

    std::string_view span_text(tokens_t s) const noexcept {
        if (s.empty())
            return {};
        const auto from = s.front().offset;
        return text.substr(from, s.back().offset + s.back().size - from);
    }

    void include(const token& t) {
        auto line = of(t).substr(1);
        std::size_t end = 0;
        while (end < line.size() && !is_space(line[end]))
            ++end;
        const auto command = line.substr(0, end);
        const bool relative = command == "ir" ||
            command == "include_relative";
        if (!relative && command != "i" && command != "include")
            return;
        line.remove_prefix(end);
        std::size_t from = 0;
        while (from < line.size() && is_space(line[from]))
            ++from;
        if (from == 0 || from == line.size())
            return;
        auto to = from;
        while (to < line.size() && !is_space(line[to]))
            ++to;
        auto path = line.substr(from, to - from);
        if (path.size() >= 2 && path.front() == '\'' && path.back() == '\'')
            path = path.substr(1, path.size() - 2);
        out.includes.push_back({relative, std::string(path), t.offset});
    }

    void calls(tokens_t s) {
        for (std::size_t i = 0; i + 1 < s.size(); ++i) {
            if (s[i].kind != token_kind::word || !symbol(s, i + 1, '('))
                continue;
            const auto word = of(s[i]);
            if (word.size() < 3 || !is_keyword(word.substr(word.size() - 3),
                    "_fn"))
                continue;
            const auto close = closing(s, i + 1);
            if (close == npos) {
                out.unbalanced.push_back({s[i].offset, true});
                return;
            }
            sql_call c{lowered(word), s[i].offset, {}};
            bool literal = false;
            for (const auto e : elements(s, i + 1, close)) {
                if (e.size() == 1 && e[0].kind == token_kind::string) {
                    c.arguments.emplace_back(string_value(of(e[0])));
                    literal = true;
                } else {
                    c.arguments.emplace_back();
                }
            }
            if (literal)
                out.calls.push_back(std::move(c));
        }
    }

    void table(tokens_t s, std::size_t i) {
        if (is(s, i, "if") && is(s, i + 1, "not") && is(s, i + 2, "exists"))
            i += 3;
        auto [name, after] = qualified(s, i);
        if (after == npos || !symbol(s, after, '('))
            return;
        const auto close = closing(s, after);
        if (close == npos)
            return;
        sql_table t{std::move(name), s.front().offset, {}, {}};
        static constexpr std::array<std::string_view, 7> constraints{
            "constraint", "primary", "unique", "foreign", "check", "exclude",
            "like"};
        static constexpr std::array<std::string_view, 11> column_ends{
            "not", "null", "default", "primary", "references", "check",
            "unique", "constraint", "generated", "collate", "foreign"};
        for (const auto e : elements(s, after, close)) {
            if (e.empty())
                continue;
            if (e[0].kind == token_kind::word &&
                std::ranges::any_of(constraints,
                    [&](auto k) { return is(e[0], k); })) {
                for (std::size_t k = 0; k + 2 < e.size(); ++k) {
                    if (!is(e[k], "primary") || !is(e[k + 1], "key") ||
                        !symbol(e, k + 2, '('))
                        continue;
                    const auto end = closing(e, k + 2);
                    if (end == npos)
                        break;
                    for (const auto c : elements(e, k + 2, end)) {
                        if (!c.empty())
                            t.primary_key.push_back(
                                name_of(of(c[0]), c[0].kind));
                    }
                    break;
                }
                continue;
            }
            if (!names(e, 0))
                continue;
            auto column = name_of(of(e[0]), e[0].kind);
            std::size_t type_end = 1;
            while (type_end < e.size() && !std::ranges::any_of(column_ends,
                       [&](auto k) { return is(e[type_end], k); }))
                ++type_end;
            for (std::size_t k = type_end; k + 1 < e.size(); ++k) {
                if (is(e[k], "primary") && is(e[k + 1], "key"))
                    t.primary_key.push_back(column);
            }
            t.columns.push_back({std::move(column),
                collapsed(span_text(e.subspan(1, type_end - 1)))});
        }
        out.tables.push_back(std::move(t));
    }

    void index(tokens_t s, std::size_t i, bool unique) {
        sql_index x;
        x.unique = unique;
        x.offset = s.front().offset;
        if (is(s, i, "concurrently"))
            ++i;
        if (is(s, i, "if") && is(s, i + 1, "not") && is(s, i + 2, "exists")) {
            x.if_not_exists = true;
            i += 3;
        }
        if (!is(s, i, "on")) {
            if (!names(s, i))
                return;
            x.name = name_of(of(s[i]), s[i].kind);
            ++i;
        }
        if (!is(s, i, "on"))
            return;
        ++i;
        if (is(s, i, "only"))
            ++i;
        auto [table, after] = qualified(s, i);
        if (after == npos)
            return;
        if (is(s, after, "using"))
            after += 2;
        if (!symbol(s, after, '('))
            return;
        const auto close = closing(s, after);
        if (close == npos)
            return;
        x.table = std::move(table);
        x.columns = collapsed(span_text(s.subspan(after + 1,
            close - after - 1)));
        out.indexes.push_back(std::move(x));
    }

    void policy(tokens_t s, std::size_t i) {
        if (!names(s, i) || !is(s, i + 1, "on"))
            return;
        auto [table, after] = qualified(s, i + 2);
        if (after == npos)
            return;
        sql_policy p{name_of(of(s[i]), s[i].kind), std::move(table),
            s.front().offset, false, false, false};
        static constexpr std::string_view current =
            "ores_iam_current_tenant_id_fn";
        static constexpr std::string_view system =
            "ores_utility_system_tenant_id_fn";
        const auto call_at = [&](std::size_t k, std::string_view name) {
            return is(s, k, name) && symbol(s, k + 1, '(') &&
                symbol(s, k + 2, ')');
        };
        for (auto k = after; k < s.size(); ++k) {
            if (is(s[k], "as") && is(s, k + 1, "restrictive"))
                p.restrictive = true;
            if (s[k].kind == token_kind::word &&
                lowered(of(s[k])).contains("party_id"))
                p.names_party_id = true;
            if (symbol(s, k + 3, '=') &&
                ((call_at(k, current) && call_at(k + 4, system)) ||
                    (call_at(k, system) && call_at(k + 4, current))))
                p.widens_to_system_tenant = true;
        }
        out.policies.push_back(std::move(p));
    }

    /**
     * @brief The function the statement defines, or nullptr.
     */
    sql_function* function(tokens_t s, std::size_t i) {
        auto [name, after] = qualified(s, i);
        if (after == npos || !symbol(s, after, '('))
            return nullptr;
        const auto close = closing(s, after);
        if (close == npos) {
            out.unbalanced.push_back({s[after].offset, false});
            return nullptr;
        }
        sql_function f{std::move(name), s.front().offset, {}, {}, {}, {}};
        for (const auto e : elements(s, after, close)) {
            if (!e.empty())
                f.parameters.push_back(lowered(of(e[0])));
        }
        out.functions.push_back(std::move(f));
        return &out.functions.back();
    }

    void row_level_security(tokens_t s) {
        std::size_t i = 2;
        if (is(s, i, "if") && is(s, i + 1, "exists"))
            i += 2;
        if (is(s, i, "only"))
            ++i;
        auto [table, after] = qualified(s, i);
        if (after != npos && is(s, after, "enable") &&
            is(s, after + 1, "row") && is(s, after + 2, "level") &&
            is(s, after + 3, "security"))
            out.row_level_security.push_back(std::move(table));
    }

    /**
     * @brief Where the statement selects from a table with a where clause:
     * the table and the column = p_param pairs of that clause.
     */
    std::optional<sql_guard> selected(tokens_t s) const {
        std::size_t k = 0;
        while (k < s.size() && !is(s[k], "select"))
            ++k;
        for (; k + 2 < s.size(); ++k) {
            if (!is(s[k], "from") || s[k + 1].kind != token_kind::word ||
                !is(s[k + 2], "where"))
                continue;
            sql_guard g{lowered(of(s[k + 1])), {}};
            for (auto j = k + 3; j + 2 < s.size(); ++j) {
                if (s[j].kind != token_kind::word || !symbol(s, j + 1, '=') ||
                    s[j + 2].kind != token_kind::word)
                    continue;
                auto param = lowered(of(s[j + 2]));
                if (param.starts_with("p_"))
                    g.columns.emplace_back(lowered(of(s[j])),
                        std::move(param));
            }
            return g;
        }
        return std::nullopt;
    }

    std::size_t raises_not_found(tokens_t s) const {
        std::size_t n = 0;
        for (std::size_t k = 0; k + 2 < s.size(); ++k) {
            if (!is(s[k], "raise") || !is(s[k + 1], "exception") ||
                s[k + 2].kind != token_kind::string)
                continue;
            const auto message = lowered(of(s[k + 2]));
            for (auto at = message.find("not"); at != std::string::npos;
                 at = message.find("not", at + 1)) {
                auto j = at + 3;
                if (j >= message.size() || !is_space(message[j]))
                    continue;
                while (j < message.size() && is_space(message[j]))
                    ++j;
                if (message.compare(j, 5, "found") == 0) {
                    ++n;
                    break;
                }
            }
        }
        return n;
    }

    void body(tokens_t s, sql_function& f) {
        for (std::size_t k = 0; k + 1 < s.size(); ++k) {
            std::size_t t = npos;
            if (is(s[k], "insert") && is(s, k + 1, "into"))
                t = k + 2;
            else if (is(s[k], "delete") && is(s, k + 1, "from"))
                t = k + 2;
            else if (is(s[k], "update"))
                t = k + 1;
            if (t >= s.size() || s[t].kind != token_kind::word)
                continue;
            const auto table = lowered(of(s[t]));
            if (table.starts_with("ores_") && table.ends_with("_tbl") &&
                table.size() >= 9)
                f.writes.push_back(table);
        }
        for (std::size_t k = 0; k + 3 < s.size(); ++k) {
            if (!is(s[k], "insert") || !is(s[k + 1], "into") ||
                s[k + 2].kind != token_kind::word || !symbol(s, k + 3, '('))
                continue;
            const auto columns_close = closing(s, k + 3);
            if (columns_close == npos || !is(s, columns_close + 1, "values") ||
                !symbol(s, columns_close + 2, '('))
                continue;
            const auto values_close = closing(s, columns_close + 2);
            if (values_close == npos)
                continue;
            const auto columns = elements(s, k + 3, columns_close);
            const auto values = elements(s, columns_close + 2, values_close);
            if (columns.size() != values.size())
                continue;
            sql_insert in{lowered(of(s[k + 2])), {}};
            for (std::size_t c = 0; c < columns.size(); ++c)
                in.values.emplace_back(lowered(span_text(columns[c])),
                    std::string(span_text(values[c])));
            f.inserts.push_back(std::move(in));
        }
    }

    /**
     * @brief The function, if any, whose body the dollar bodies of the
     * statement are.
     */
    std::optional<std::size_t> statement(tokens_t s, sql_function* owner) {
        calls(s);
        if (owner != nullptr)
            body(s, *owner);
        // In a PL/pgSQL body a statement may follow begin, then or else
        // with no semicolon between, so the DDL starts where its keyword is.
        for (std::size_t k = 0; k < s.size(); ++k) {
            if (is(s[k], "alter") && is(s, k + 1, "table")) {
                row_level_security(s.subspan(k));
                return std::nullopt;
            }
            if (is(s[k], "create"))
                return ddl(s.subspan(k));
        }
        return std::nullopt;
    }

    std::optional<std::size_t> ddl(tokens_t s) {
        std::size_t i = 1;
        if (is(s, i, "or") && is(s, i + 1, "replace"))
            i += 2;
        if (is(s, i, "table"))
            table(s, i + 1);
        else if (is(s, i, "unique") && is(s, i + 1, "index"))
            index(s, i + 2, true);
        else if (is(s, i, "index"))
            index(s, i + 1, false);
        else if (is(s, i, "policy"))
            policy(s, i + 1);
        else if (is(s, i, "function")) {
            if (function(s, i + 1) != nullptr)
                return out.functions.size() - 1;
        }
        return std::nullopt;
    }

    void walk(tokens_t tokens, std::optional<std::size_t> owner) {
        std::vector<token> code;
        std::optional<sql_guard> last_select;
        const auto flush = [&]() {
            const tokens_t s(code);
            auto* f = owner ? &out.functions[*owner] : nullptr;
            if (f != nullptr) {
                const auto raised = raises_not_found(s);
                for (std::size_t r = 0; r < raised && last_select; ++r)
                    f->guards.push_back(*last_select);
            }
            const auto defined = statement(s, f);
            if (f != nullptr) {
                if (auto g = selected(s))
                    last_select = std::move(g);
            }
            for (const auto& t : s) {
                if (t.kind != token_kind::dollar_body)
                    continue;
                const auto content = dollar_content(of(t));
                const auto base = static_cast<std::uint32_t>(
                    content.data() - text.data());
                const auto inner = lex(content, base, false);
                walk(inner.tokens, defined ? defined : owner);
            }
            code.clear();
        };
        for (const auto& t : tokens) {
            if (is_comment(t))
                continue;
            if (t.kind == token_kind::meta) {
                flush();
                include(t);
                continue;
            }
            if (t.kind == token_kind::symbol && text[t.offset] == ';') {
                flush();
                continue;
            }
            code.push_back(t);
        }
        flush();
    }
};

}

sql_file model_of(std::string_view text, const lexed& tokens) {
    sql_file out;
    walker w{text, out};
    w.walk(tokens.tokens, std::nullopt);
    return out;
}

}
