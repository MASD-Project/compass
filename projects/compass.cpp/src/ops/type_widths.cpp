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
#include "compass.cpp/ops/type_widths.hpp"

#include <algorithm>
#include <array>

namespace compass::cpp::ops {

namespace {

constexpr std::array<std::string_view, 3> class_keys{"class", "struct",
    "union"};

/**
 * @brief The first words of a member declaration that declares no data
 * member of the object.
 */
constexpr std::array<std::string_view, 10> not_data{"using", "typedef",
    "friend", "static", "template", "static_assert", "enum", "operator",
    "virtual", "explicit"};

constexpr std::array<std::string_view, 4> access_words{"public", "private",
    "protected", "signals"};

class reader {
public:
    explicit reader(const lexed& l) : code_(l.code), t_(tokens_of(l)) {}

    std::vector<type_width> widths() {
        std::vector<type_width> out;
        for (std::size_t k = 0; k < t_.size(); ++k) {
            if (!is_class_key(k) || (k > 0 && is(k - 1, "enum")))
                continue;
            auto name = skip_attributes(k + 1);
            if (is_name(name) && is_name(name + 1) && !is(name + 1, "final"))
                ++name;
            if (!is_name(name) || is_class_key(name))
                continue;
            const auto open = body_of(name + 1);
            if (open == 0)
                continue;
            out.push_back({text(k), text(name), t_[k].begin,
                fields(open + 1, close_of(open))});
        }
        return out;
    }

private:
    std::string_view text(std::size_t k) const {
        return std::string_view(code_).substr(t_[k].begin,
            t_[k].end - t_[k].begin);
    }

    bool is(std::size_t k, std::string_view s) const {
        return k < t_.size() && t_[k].kind != token_kind::literal &&
            text(k) == s;
    }

    bool is_name(std::size_t k) const {
        return k < t_.size() && t_[k].kind == token_kind::name;
    }

    bool is_class_key(std::size_t k) const {
        return is_name(k) && std::ranges::contains(class_keys, text(k));
    }

    /**
     * @brief A macro, as the code writes one: upper case, digits and an
     * underscore, as in Q_OBJECT.
     */
    bool is_macro(std::size_t k) const {
        if (!is_name(k))
            return false;
        const auto s = text(k);
        return s.size() > 2 && s.contains('_') && std::ranges::all_of(s,
            [](char c) { return (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_'; });
    }

    std::size_t skip_attributes(std::size_t k) const {
        for (;;) {
            if (is(k, "[") && is(k + 1, "["))
                k = past(k, "[", "]");
            else if ((is(k, "alignas") || is(k, "__declspec")) &&
                     is(k + 1, "("))
                k = past(k + 1, "(", ")");
            else
                return k;
        }
    }

    std::size_t past(std::size_t k, std::string_view open,
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
     * @brief The { that opens the body of the type named at @p k - 1, or
     * 0 when the name does not start a definition: its template
     * arguments, final and a base clause may come between them, and
     * nothing that ends or continues a declaration, a declarator or an
     * expression.
     */
    std::size_t body_of(std::size_t k) const {
        if (is(k, "<"))
            k = past(k, "<", ">");
        for (; k < t_.size(); ++k) {
            if (is(k, "{"))
                return k;
            if (is(k, "<")) {
                k = past(k, "<", ">") - 1;
                continue;
            }
            if (is_name(k) || is(k, "::") || is(k, ":") || is(k, ","))
                continue;
            return 0;
        }
        return 0;
    }

    std::size_t close_of(std::size_t open) const {
        return past(open, "{", "}") - 1;
    }

    /**
     * @brief The data members declared from @p k to @p end at the depth
     * of the body.
     */
    std::uint32_t fields(std::size_t k, std::size_t end) const {
        std::uint32_t n = 0;
        while (k < end) {
            k = skip_labels_and_macros(k, end);
            if (k >= end)
                break;
            if (is(k, "{") || is(k, ";")) {
                k = is(k, ";") ? k + 1 : past(k, "{", "}");
                continue;
            }
            const auto [declared, next] = member(k, end);
            n += declared;
            k = next;
        }
        return n;
    }

    std::size_t skip_labels_and_macros(std::size_t k, std::size_t end) const {
        for (;;) {
            auto j = k;
            if (is_name(j) && std::ranges::contains(access_words, text(j))) {
                while (j < end && is_name(j))
                    ++j;
                if (is(j, ":")) {
                    k = j + 1;
                    continue;
                }
            }
            if (k < end && is_macro(k)) {
                k = is(k + 1, "(") ? past(k + 1, "(", ")") : k + 1;
                continue;
            }
            return k;
        }
    }

    /**
     * @brief Past a constructor's member initialisers from @p k, each a
     * name and a parenthesised or braced list, up to its body.
     */
    std::size_t past_initialisers(std::size_t k, std::size_t end) const {
        while (k < end) {
            while (k < end && (is_name(k) || is(k, "::")))
                ++k;
            if (is(k, "<"))
                k = past(k, "<", ">");
            if (is(k, "("))
                k = past(k, "(", ")");
            else if (is(k, "{"))
                k = past(k, "{", "}");
            if (!is(k, ","))
                return k;
            ++k;
        }
        return k;
    }

    /**
     * @brief How many data members the declaration at @p k declares, and
     * where the next one starts. A declaration with a parameter list
     * before any initialiser is a function: it ends at its ; or past its
     * body. A nested type ends at the ; after its body.
     */
    std::pair<std::uint32_t, std::size_t> member(std::size_t k,
        std::size_t end) const {
        const auto first = k;
        bool data = !std::ranges::contains(not_data, text(k)) &&
            !is(k, "~");
        const bool nested = is_class_key(k);
        bool function = false;
        bool initialising = false;
        std::uint32_t declarators = 1;
        int depth = 0;
        int angles = 0;
        for (; k < end; ++k) {
            if (depth == 0 && is(k, ";"))
                break;
            if (is(k, "(") || is(k, "[")) {
                if (depth == 0 && angles == 0 && !initialising &&
                    is(k, "("))
                    function = true;
                ++depth;
            } else if (is(k, ")") || is(k, "]")) {
                --depth;
            } else if (depth == 0 && function && !nested && is(k, ":")) {
                k = past_initialisers(k + 1, end) - 1;
            } else if (is(k, "{")) {
                if (depth == 0 && function && !nested)
                    return {0, past(k, "{", "}")};
                ++depth;
            } else if (is(k, "}")) {
                --depth;
            } else if (depth == 0 && !initialising) {
                if (is(k, "<") && k > first && (is_name(k - 1) ||
                        is(k - 1, ">")))
                    ++angles;
                else if (is(k, ">") && angles > 0)
                    --angles;
                else if (angles == 0 && (is(k, "=") || is(k, ":")))
                    initialising = true;
                else if (angles == 0 && is(k, ","))
                    ++declarators;
                else if (is_name(k) && (text(k) == "static" ||
                         text(k) == "operator"))
                    data = false;
            } else if (depth == 0 && angles == 0 && is(k, ",")) {
                ++declarators;
            }
        }
        const auto next = k < end ? k + 1 : end;
        if (!data || function || (nested && k - first <= 2))
            return {0, next};
        if (nested) {
            const auto body = std::ranges::find_if(t_.begin() +
                static_cast<std::ptrdiff_t>(first), t_.begin() +
                static_cast<std::ptrdiff_t>(k), [&](const token& t) {
                    return t.kind == token_kind::punct &&
                        std::string_view(code_).substr(t.begin, 1) == "{"; });
            if (body != t_.begin() + static_cast<std::ptrdiff_t>(k))
                return {0, next};
        }
        return {declarators, next};
    }

    std::string_view code_;
    std::vector<token> t_;
};

}

std::vector<type_width> type_widths(const lexed& l) {
    return reader(l).widths();
}

}
