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
#include "compass.cpp/ops/source_model.hpp"

#include <algorithm>
#include <array>
#include <optional>

namespace compass::cpp::ops {

namespace {

constexpr bool is_name_char(char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '$' ||
        static_cast<unsigned char>(c) >= 0x80;
}

constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }

constexpr bool is_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v' ||
        c == '\n';
}

enum class token_kind : std::uint8_t { name, number, literal, punct };

struct token {
    token_kind kind;
    std::uint32_t begin;
    std::uint32_t end;
};

/**
 * @brief The tokens of the live code outside directives. :: is one
 * token; every other punctuator is one byte.
 */
std::vector<token> tokens_of(const lexed& l) {
    const std::string_view code(l.code);
    const auto n = static_cast<std::uint32_t>(code.size());
    std::vector<token> out;
    out.reserve(code.size() / 6);
    std::size_t di = 0, xi = 0, li = 0;
    for (std::uint32_t i = 0; i < n;) {
        if (di < l.directives.size() && l.directives[di].begin <= i) {
            i = std::max(i, l.directives[di++].end);
            continue;
        }
        if (xi < l.dead.size() && l.dead[xi].begin <= i) {
            i = std::max(i, l.dead[xi++].end);
            continue;
        }
        while (li < l.literals.size() && l.literals[li].end <= i)
            ++li;
        if (li < l.literals.size() && l.literals[li].begin == i) {
            out.push_back({token_kind::literal, i, l.literals[li].end});
            i = l.literals[li++].end;
            continue;
        }
        const char c = code[i];
        if (is_space(c)) {
            ++i;
            continue;
        }
        if (is_name_char(c)) {
            auto e = i + 1;
            while (e < n && is_name_char(code[e]))
                ++e;
            out.push_back({is_digit(c) ? token_kind::number : token_kind::name,
                i, e});
            i = e;
            continue;
        }
        if (c == ':' && i + 1 < n && code[i + 1] == ':') {
            out.push_back({token_kind::punct, i, i + 2});
            i += 2;
            continue;
        }
        out.push_back({token_kind::punct, i, i + 1});
        ++i;
    }
    return out;
}

constexpr std::array<std::string_view, 15> leading_specifiers{"static",
    "inline", "const", "constexpr", "consteval", "constinit", "extern",
    "thread_local", "virtual", "explicit", "mutable", "volatile", "register",
    "friend", "typename"};

constexpr std::array<std::string_view, 6> operators{"decltype", "alignas",
    "sizeof", "noexcept", "__attribute__", "__declspec"};

constexpr std::array<std::string_view, 9> after_parameters{"const",
    "noexcept", "override", "final", "volatile", "mutable", "try",
    "requires", "throw"};

class reader {
public:
    reader(std::string_view source, const lexed& l, model_parts parts,
        source_model& m)
        : source_(source), code_(l.code), l_(l), m_(m), parts_(parts),
          t_(wants(model_parts::names | model_parts::heads |
              model_parts::declarations) ? tokens_of(l)
                  : std::vector<token>{}) {}

    void run() {
        directives();
        if (wants(model_parts::names | model_parts::heads))
            names_and_heads();
        if (wants(model_parts::declarations))
            declarations();
    }

private:
    bool wants(model_parts p) const noexcept {
        return (parts_ & p) != model_parts::directives;
    }

    std::string_view text(const token& t) const {
        return code_.substr(t.begin, t.end - t.begin);
    }

    bool is(std::size_t k, std::string_view s) const {
        return k < t_.size() && t_[k].kind != token_kind::literal &&
            text(t_[k]) == s;
    }

    bool is_name(std::size_t k) const {
        return k < t_.size() && t_[k].kind == token_kind::name;
    }

    void directives() {
        for (const auto d : l_.directives) {
            if (l_.is_dead(d.begin))
                continue;
            const auto p = parts_of(l_, d);
            if (p.keyword == "include" || p.keyword == "include_next") {
                std::size_t i = 0;
                while (i < p.rest.size() && is_space(source_[p.rest_offset +
                           i]))
                    ++i;
                const auto at = p.rest_offset + static_cast<std::uint32_t>(i);
                if (i < p.rest.size() && p.rest[i] == '<') {
                    const auto close = p.rest.find('>', i);
                    if (close != std::string_view::npos)
                        m_.includes.push_back({p.rest.substr(i + 1,
                            close - i - 1), true, d.begin});
                    continue;
                }
                const auto lit = std::ranges::lower_bound(l_.literals, at,
                    {}, &span::begin);
                if (lit != l_.literals.end() && lit->begin == at &&
                    source_[at] == '"' && lit->end - lit->begin >= 2)
                    m_.includes.push_back({source_.substr(at + 1,
                        lit->end - at - 2), false, d.begin});
            } else if (p.keyword == "define") {
                std::size_t i = 0;
                while (i < p.rest.size() && is_space(p.rest[i]))
                    ++i;
                auto e = i;
                while (e < p.rest.size() && is_name_char(p.rest[e]))
                    ++e;
                if (e == i)
                    continue;
                auto body = e;
                if (body < p.rest.size() && p.rest[body] == '(') {
                    const auto close = p.rest.find(')', body);
                    body = close == std::string_view::npos ? p.rest.size()
                        : close + 1;
                }
                m_.macros.push_back({p.rest.substr(i, e - i),
                    p.rest.substr(body), body != e, d.begin});
            }
        }
    }

    void names_and_heads() {
        for (std::size_t k = 0; k < t_.size(); ++k) {
            const bool starts_with_scope = is(k, "::") && is_name(k + 1) &&
                !(k > 0 && (is_name(k - 1) || is(k - 1, ">")));
            if (!is_name(k) && !starts_with_scope)
                continue;
            if (is_name(k) && k > 0 && is(k - 1, "::"))
                continue;
            auto e = starts_with_scope ? k + 1 : k;
            while (is(e + 1, "::") && is_name(e + 2))
                e += 2;
            if (wants(model_parts::names))
                m_.names.push_back({code_.substr(t_[k].begin,
                    t_[e].end - t_[k].begin), t_[k].begin, is(e + 1, "(")});
            const bool enum_head = is(k, "enum") &&
                !(k > 0 && is(k - 1, "using"));
            const bool class_head = (is(k, "class") || is(k, "struct") ||
                is(k, "union")) && !(k > 0 && is(k - 1, "enum"));
            if (wants(model_parts::heads) && (enum_head || class_head))
                head(k);
            k = e;
        }
    }

    void head(std::size_t k) {
        const bool is_enum = is(k, "enum");
        auto j = k + 1;
        if (is_enum && (is(j, "class") || is(j, "struct")))
            ++j;
        j = skip_attributes(j);
        if (!is_name(j))
            return;
        std::string_view macro;
        auto name = j;
        if (is_name(j + 1)) {
            macro = text(t_[j]);
            name = j + 1;
        }
        if (!is_enum || is(name + 1, ":") || is(name + 1, "{") ||
            is(name + 1, ";"))
            m_.heads.push_back({t_[k].begin, text(t_[k]), macro,
                text(t_[name])});
    }

    /**
     * @brief Past [[...]], alignas(...), __attribute__((...)) and
     * __declspec(...) at @p k.
     */
    std::size_t skip_attributes(std::size_t k) const {
        for (;;) {
            if (is(k, "[") && is(k + 1, "[")) {
                k = skip_balanced(k, "[", "]");
            } else if ((is(k, "alignas") || is(k, "__attribute__") ||
                           is(k, "__declspec")) && is(k + 1, "(")) {
                k = skip_balanced(k + 1, "(", ")");
            } else {
                return k;
            }
        }
    }

    /**
     * @brief Past the token that closes the one at @p k.
     */
    std::size_t skip_balanced(std::size_t k, std::string_view open,
        std::string_view close) const {
        int depth = 0;
        for (; k < t_.size(); ++k) {
            if (is(k, open))
                ++depth;
            else if (is(k, close) && --depth == 0)
                return k + 1;
        }
        return k;
    }

    /**
     * @brief Past the ; that ends the statement at @p k, or at the } that
     * closes its scope.
     */
    std::size_t skip_statement(std::size_t k) const {
        int depth = 0;
        for (; k < t_.size(); ++k) {
            if (is(k, "(") || is(k, "[") || is(k, "{")) {
                ++depth;
            } else if (is(k, ")") || is(k, "]") || is(k, "}")) {
                if (depth == 0)
                    return k;
                --depth;
            } else if (is(k, ";") && depth == 0) {
                return k + 1;
            }
        }
        return k;
    }

    std::string scope() const {
        std::string out;
        for (const auto& s : scopes_) {
            if (s.empty())
                continue;
            if (!out.empty())
                out += "::";
            out += s;
        }
        return out;
    }

    bool anonymous() const {
        return std::ranges::any_of(scopes_,
            [](const auto& s) { return s.empty(); });
    }

    void declarations() {
        for (std::size_t k = 0; k < t_.size();) {
            if (is(k, "}")) {
                if (!scopes_.empty())
                    scopes_.pop_back();
                ++k;
            } else if (is(k, ";")) {
                ++k;
            } else if (is(k, "{")) {
                k = skip_balanced(k, "{", "}");
            } else if (is(k, "namespace") || (is(k, "inline") &&
                           is(k + 1, "namespace"))) {
                k = name_space(is(k, "inline") ? k + 1 : k);
            } else if (is(k, "extern") &&
                       k + 1 < t_.size() &&
                       t_[k + 1].kind == token_kind::literal) {
                k = is(k + 2, "{") ? skip_balanced(k + 2, "{", "}")
                    : statement(k + 2, true);
            } else if (is(k, "using") || is(k, "typedef") ||
                       is(k, "static_assert") || is(k, "asm") ||
                       is(k, "export")) {
                k = skip_statement(k);
            } else {
                k = statement(k);
            }
        }
    }

    std::size_t name_space(std::size_t k) {
        auto j = k + 1;
        std::string name;
        while (is_name(j) || is(j, "::")) {
            name += text(t_[j]);
            ++j;
        }
        j = skip_attributes(j);
        if (!is(j, "{"))
            return skip_statement(k);
        scopes_.push_back(std::move(name));
        return j + 1;
    }

    std::size_t statement(std::size_t k, bool linkage_named = false) {
        declaration d;
        d.is_extern = linkage_named;
        d.start = t_[k].begin;
        if (is(k, "template") && is(k + 1, "<")) {
            d.is_template = true;
            k = skip_balanced(k + 1, "<", ">");
            if (k == t_.size() || is(k, ";"))
                return k;
            d.start = t_[k].begin;
        }
        for (;;) {
            k = skip_attributes(k);
            if (!is_name(k))
                break;
            const auto w = text(t_[k]);
            if (std::ranges::find(leading_specifiers, w) ==
                leading_specifiers.end())
                break;
            d.is_static = d.is_static || w == "static";
            d.is_inline = d.is_inline || w == "inline";
            d.is_const = d.is_const || w == "const";
            d.is_constexpr = d.is_constexpr || w == "constexpr";
            d.is_extern = d.is_extern || w == "extern";
            ++k;
        }
        if (is(k, "class") || is(k, "struct") || is(k, "union") ||
            is(k, "enum")) {
            if (const auto after = type(k, d))
                return *after;
        }
        return other(k, d);
    }

    /**
     * @brief Reads the class, struct, union or enum at @p k, or nothing
     * when what follows the class key is not its name: a specialisation,
     * or an elaborated type in another declaration.
     */
    std::optional<std::size_t> type(std::size_t k, declaration& d) {
        const bool is_enum = is(k, "enum");
        auto j = k + 1;
        bool scoped = !is_enum;
        if (is_enum && (is(j, "class") || is(j, "struct"))) {
            scoped = true;
            ++j;
        }
        j = skip_attributes(j);
        if (is_name(j) && is_name(j + 1) && !is(j + 1, "final")) {
            d.macro = text(t_[j]);
            ++j;
        }
        if (!is_name(j)) {
            if (is(j, "{") || is(j, ":"))
                return skip_statement(skip_to_brace(j));
            return std::nullopt;
        }
        while (is(j + 1, "::") && is_name(j + 2)) {
            j += 2;
            d.qualified = true;
        }
        d.name = text(t_[j]);
        d.offset = t_[j].begin;
        d.is_enum = is_enum;
        d.kind = scoped ? declaration_kind::type
            : declaration_kind::enumeration;
        auto e = j + 1;
        if (is(e, "<")) {
            // A specialisation declares no name of its own.
            const auto brace = skip_to_brace(skip_balanced(e, "<", ">"));
            return is(brace, "{")
                ? skip_statement(skip_balanced(brace, "{", "}"))
                : skip_statement(brace);
        }
        if (is(e, "final"))
            ++e;
        if (is(e, ";")) {
            add(d);
            return e + 1;
        }
        if (!is(e, "{") && !is(e, ":"))
            return std::nullopt;
        const auto brace = skip_to_brace(e);
        if (!is(brace, "{"))
            return skip_statement(e);
        d.definition = true;
        add(d);
        return skip_statement(skip_balanced(brace, "{", "}"));
    }

    /**
     * @brief The { at or after @p k that opens a body, past base lists and
     * initialisers; or the ; or } that comes first.
     */
    std::size_t skip_to_brace(std::size_t k) const {
        for (; k < t_.size(); ++k) {
            if (is(k, "{") || is(k, ";") || is(k, "}"))
                return k;
            if (is(k, "("))
                k = skip_balanced(k, "(", ")") - 1;
        }
        return k;
    }

    std::size_t other(std::size_t k, declaration& d) {
        int angles = 0;
        std::size_t name = t_.size();
        for (auto j = k; j < t_.size(); ++j) {
            if (is(j, "<")) {
                ++angles;
            } else if (is(j, ">")) {
                angles = std::max(0, angles - 1);
            } else if (angles > 0) {
                if (is(j, ";") || is(j, "{") || is(j, "}"))
                    angles = 0;
                else
                    continue;
            }
            if (is_name(j) && text(t_[j]) == "operator") {
                name = j;
                auto p = j + 1;
                if (is(p, "(") && is(p + 1, ")"))
                    p += 2;
                while (p < t_.size() && !is(p, "(") && !is(p, ";"))
                    ++p;
                return function(p, name, d);
            }
            if (is_name(j)) {
                name = j;
                continue;
            }
            if (is(j, "[") && name != t_.size()) {
                j = skip_balanced(j, "[", "]") - 1;
                continue;
            }
            if (is(j, "(") && name != t_.size() && name + 1 == j &&
                std::ranges::find(operators, text(t_[name])) ==
                    operators.end())
                return function(j, name, d);
            if (is(j, "(")) {
                j = skip_balanced(j, "(", ")") - 1;
                continue;
            }
            if ((is(j, "=") || is(j, "{") || is(j, ";")) &&
                name != t_.size()) {
                d.kind = declaration_kind::variable;
                set_name(d, name, name);
                d.definition = !d.is_extern || !is(j, ";");
                add(d);
                if (is(j, "{"))
                    return skip_statement(skip_balanced(j, "{", "}"));
                return skip_statement(j);
            }
            if (is(j, ";") || is(j, "{") || is(j, "}"))
                return is(j, "}") ? j : skip_statement(j);
        }
        return t_.size();
    }

    void set_name(declaration& d, std::size_t first, std::size_t last) {
        d.name = code_.substr(t_[first].begin, t_[last].end - t_[first].begin);
        d.offset = t_[first].begin;
        d.qualified = first > 0 && is(first - 1, "::");
    }

    /**
     * @brief Reads a function whose name runs from @p name to the token
     * before @p paren, its parameters opening there.
     */
    std::size_t function(std::size_t paren, std::size_t name,
        declaration& d) {
        d.kind = declaration_kind::function;
        set_name(d, name, paren - 1);
        auto j = skip_balanced(paren, "(", ")");
        if (j > paren + 1 && j <= t_.size())
            d.parameters = code_.substr(t_[paren].end,
                t_[j - 1].begin - t_[paren].end);
        for (;;) {
            j = skip_attributes(j);
            if (is_name(j) && std::ranges::find(after_parameters,
                    text(t_[j])) != after_parameters.end()) {
                ++j;
                if (is(j, "("))
                    j = skip_balanced(j, "(", ")");
            } else if (is(j, "&") || is(j, "&&")) {
                ++j;
            } else if (is(j, "-") && is(j + 1, ">")) {
                j += 2;
                while (j < t_.size() && !is(j, "{") && !is(j, ";") &&
                       !is(j, "=") && !is(j, "}"))
                    ++j;
            } else {
                break;
            }
        }
        if (is(j, ":")) {
            ++j;
            while (j < t_.size()) {
                while (is_name(j) || is(j, "::"))
                    ++j;
                if (is(j, "<"))
                    j = skip_balanced(j, "<", ">");
                if (is(j, "("))
                    j = skip_balanced(j, "(", ")");
                else if (is(j, "{"))
                    j = skip_balanced(j, "{", "}");
                else
                    break;
                if (!is(j, ","))
                    break;
                ++j;
            }
        }
        if (is(j, "{")) {
            d.definition = true;
            add(d);
            return skip_balanced(j, "{", "}");
        }
        if (is(j, ";") || is(j, "=")) {
            add(d);
            return skip_statement(j);
        }
        // A macro invoked with no ; after it ends where its ) does.
        return j;
    }

    void add(const declaration& d) {
        auto out = d;
        out.scope = scope();
        out.anonymous = anonymous();
        m_.declarations.push_back(std::move(out));
    }

    std::string_view source_;
    std::string_view code_;
    const lexed& l_;
    source_model& m_;
    model_parts parts_;
    std::vector<token> t_;
    std::vector<std::string> scopes_;
};

}

std::vector<std::string_view> components_of_name(std::string_view text) {
    std::vector<std::string_view> out;
    for (std::size_t i = 0; i < text.size();) {
        if (!is_name_char(text[i])) {
            ++i;
            continue;
        }
        auto e = i;
        while (e < text.size() && is_name_char(text[e]))
            ++e;
        out.push_back(text.substr(i, e - i));
        i = e;
    }
    return out;
}

source_model model_of(std::string_view source, const lexed& l,
    model_parts parts) {
    source_model m;
    reader(source, l, parts, m).run();
    return m;
}

}
