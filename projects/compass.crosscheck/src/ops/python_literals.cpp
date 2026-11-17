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
#include "compass.crosscheck/ops/python_literals.hpp"

#include "compass.cpp/ops/text.hpp"

namespace compass::crosscheck::ops {

namespace {

namespace co = compass::cpp::ops;

/**
 * @brief A reader of one literal: spaces, newlines and comments between
 * tokens are skipped, as they are inside brackets.
 */
class reader final {
public:
    explicit reader(std::string_view text) : text_(text) {}

    std::optional<python_value> value() {
        skip();
        if (at_end())
            return std::nullopt;
        const auto c = text_[i_];
        if (c == '"' || c == '\'' || prefix_length() > 0)
            return strings();
        if (c == '(' || c == '[' || c == '{')
            return bracketed(c);
        if (co::is_identifier(c) && !(c >= '0' && c <= '9'))
            return name_or_call();
        return std::nullopt;
    }

private:
    bool at_end() const noexcept { return i_ >= text_.size(); }

    void skip() {
        while (!at_end()) {
            if (co::is_space(text_[i_])) {
                ++i_;
            } else if (text_[i_] == '#') {
                while (!at_end() && text_[i_] != '\n')
                    ++i_;
            } else {
                return;
            }
        }
    }

    bool eat(char c) {
        skip();
        if (at_end() || text_[i_] != c)
            return false;
        ++i_;
        return true;
    }

    /**
     * @brief The length of the string prefix (r, f, b, u or a pair of
     * them) at the reader, when a quote follows it; zero otherwise.
     */
    std::size_t prefix_length() const {
        std::size_t n = 0;
        while (n < 2 && i_ + n < text_.size() &&
            std::string_view("rRfFbBuU").find(text_[i_ + n]) !=
                std::string_view::npos)
            ++n;
        if (n == 0 || i_ + n >= text_.size() ||
            (text_[i_ + n] != '"' && text_[i_ + n] != '\''))
            return 0;
        return n;
    }

    std::optional<std::string> one_string(bool raw) {
        const auto quote = text_[i_];
        const bool triple = text_.substr(i_, 3) == std::string(3, quote);
        i_ += triple ? 3 : 1;
        std::string out;
        while (!at_end()) {
            if (triple ? text_.substr(i_, 3) == std::string(3, quote)
                       : text_[i_] == quote) {
                i_ += triple ? 3 : 1;
                return out;
            }
            if (text_[i_] == '\\' && i_ + 1 < text_.size()) {
                const auto e = text_[i_ + 1];
                i_ += 2;
                if (raw) {
                    out.push_back('\\');
                    out.push_back(e);
                    continue;
                }
                switch (e) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case '\n': break;
                case '\\':
                case '\'':
                case '"': out.push_back(e); break;
                default:
                    out.push_back('\\');
                    out.push_back(e);
                    break;
                }
                continue;
            }
            out.push_back(text_[i_++]);
        }
        return std::nullopt;
    }

    std::optional<python_value> strings() {
        python_value v;
        std::string plain;
        for (;;) {
            const auto prefix = text_.substr(i_, prefix_length());
            i_ += prefix.size();
            const auto has = [&](char c) {
                return prefix.find(c) != std::string_view::npos ||
                    prefix.find(static_cast<char>(c - 'a' + 'A')) !=
                    std::string_view::npos;
            };
            const auto s = one_string(has('r'));
            if (!s)
                return std::nullopt;
            if (has('f') && v.what != python_value::kind::format) {
                v.what = python_value::kind::format;
                v.text = doubled(v.text);
            }
            v.text += has('f') || v.what != python_value::kind::format
                ? *s : doubled(*s);
            const auto saved = i_;
            skip();
            if (at_end() || (text_[i_] != '"' && text_[i_] != '\'' &&
                    prefix_length() == 0)) {
                i_ = saved;
                return v;
            }
        }
    }

    static std::string doubled(std::string_view s) {
        std::string out;
        for (const char c : s) {
            out.push_back(c);
            if (c == '{' || c == '}')
                out.push_back(c);
        }
        return out;
    }

    std::optional<python_value> bracketed(char open) {
        const char close = open == '(' ? ')' : open == '[' ? ']' : '}';
        ++i_;
        python_value v;
        v.what = open == '{' ? python_value::kind::dict
                             : python_value::kind::sequence;
        bool comma = false;
        while (!eat(close)) {
            auto item = value();
            if (!item)
                return std::nullopt;
            if (open == '{') {
                const bool tuple = item->what == python_value::kind::sequence;
                if ((item->what != python_value::kind::string && !tuple) ||
                    !eat(':'))
                    return std::nullopt;
                auto entry = value();
                if (!entry)
                    return std::nullopt;
                v.entries.push_back({tuple ? std::string() : item->text,
                    std::move(*entry),
                    tuple ? std::move(item->items)
                          : std::vector<python_value>{}});
            } else {
                v.items.push_back(std::move(*item));
            }
            if (eat(',')) {
                comma = true;
                continue;
            }
            if (!eat(close))
                return std::nullopt;
            break;
        }
        if (open == '(' && !comma && v.items.size() == 1)
            return std::move(v.items.front());
        return v;
    }

    std::optional<python_value> name_or_call() {
        python_value v;
        v.what = python_value::kind::name;
        while (!at_end() && (co::is_identifier(text_[i_]) ||
                text_[i_] == '.'))
            v.text.push_back(text_[i_++]);
        if (!eat('('))
            return v;
        v.what = python_value::kind::call;
        while (!eat(')')) {
            skip();
            const auto start = i_;
            std::string key;
            while (!at_end() && co::is_identifier(text_[i_]))
                key.push_back(text_[i_++]);
            const bool keyword = !key.empty() && eat('=') &&
                (at_end() || text_[i_] != '=');
            if (!keyword)
                i_ = start;
            auto arg = value();
            if (!arg)
                return std::nullopt;
            if (keyword)
                v.entries.push_back({std::move(key), std::move(*arg), {}});
            else
                v.items.push_back(std::move(*arg));
            if (!eat(',') && !eat(')'))
                return std::nullopt;
            if (text_[i_ - 1] == ')')
                break;
        }
        return v;
    }

    std::string_view text_;
    std::size_t i_ = 0;
};

}

std::optional<python_value> python_assignment(std::string_view source,
    std::string_view name) {
    for (std::size_t at = 0; at < source.size();) {
        auto end = source.find('\n', at);
        end = end == std::string_view::npos ? source.size() : end;
        const auto line = source.substr(at, end - at);
        if (line.starts_with(name) && line.size() > name.size() &&
            !co::is_identifier(line[name.size()])) {
            auto rest = line.substr(name.size());
            const auto equals = rest.find('=');
            const auto annotation = co::strip(rest.substr(0, equals));
            if (equals != std::string_view::npos &&
                (annotation.empty() || annotation.starts_with(':')) &&
                rest.substr(equals + 1, 1) != "=") {
                reader r(source.substr(at + name.size() + equals + 1));
                return r.value();
            }
        }
        at = end + 1;
    }
    return std::nullopt;
}

std::map<std::string, std::string, std::less<>> python_string_constants(
    std::string_view source) {
    std::map<std::string, std::string, std::less<>> out;
    for (std::size_t at = 0; at < source.size();) {
        auto end = source.find('\n', at);
        end = end == std::string_view::npos ? source.size() : end;
        const auto line = source.substr(at, end - at);
        std::size_t n = 0;
        while (n < line.size() && co::is_identifier(line[n]))
            ++n;
        if (n > 0 && !(line[0] >= '0' && line[0] <= '9')) {
            const auto name = line.substr(0, n);
            if (const auto v = python_assignment(source.substr(at), name);
                v && v->what == python_value::kind::string)
                out.insert_or_assign(std::string(name), v->text);
        }
        at = end + 1;
    }
    return out;
}

namespace {

std::optional<std::string> formatted(std::string_view tpl,
    const std::map<std::string, std::string, std::less<>>& constants) {
    std::string out;
    for (std::size_t i = 0; i < tpl.size(); ++i) {
        const char c = tpl[i];
        if ((c == '{' || c == '}') && i + 1 < tpl.size() && tpl[i + 1] == c) {
            out.push_back(c);
            ++i;
            continue;
        }
        if (c != '{') {
            out.push_back(c);
            continue;
        }
        const auto close = tpl.find('}', i);
        if (close == std::string_view::npos)
            return std::nullopt;
        const auto found = constants.find(tpl.substr(i + 1, close - i - 1));
        if (found == constants.end())
            return std::nullopt;
        out += found->second;
        i = close;
    }
    return out;
}

}

std::optional<python_value> resolved(const python_value& v,
    const std::map<std::string, std::string, std::less<>>& constants) {
    python_value out;
    out.what = v.what;
    out.text = v.text;
    if (v.what == python_value::kind::name) {
        if (const auto found = constants.find(v.text);
            found != constants.end()) {
            out.what = python_value::kind::string;
            out.text = found->second;
        }
        return out;
    }
    if (v.what == python_value::kind::format) {
        auto text = formatted(v.text, constants);
        if (!text)
            return std::nullopt;
        out.what = python_value::kind::string;
        out.text = std::move(*text);
        return out;
    }
    for (const auto& item : v.items) {
        auto r = resolved(item, constants);
        if (!r)
            return std::nullopt;
        out.items.push_back(std::move(*r));
    }
    for (const auto& e : v.entries) {
        auto value = resolved(e.value, constants);
        if (!value)
            return std::nullopt;
        python_entry entry{e.key, std::move(*value), {}};
        for (const auto& k : e.key_items) {
            auto r = resolved(k, constants);
            if (!r)
                return std::nullopt;
            entry.key_items.push_back(std::move(*r));
        }
        out.entries.push_back(std::move(entry));
    }
    return out;
}

std::optional<std::vector<std::string>> strings_of(const python_value& v) {
    if (v.what != python_value::kind::sequence)
        return std::nullopt;
    std::vector<std::string> out;
    for (const auto& item : v.items) {
        if (item.what != python_value::kind::string)
            return std::nullopt;
        out.push_back(item.text);
    }
    return out;
}

}
