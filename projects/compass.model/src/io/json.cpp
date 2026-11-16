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
#include "compass.model/io/json.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace compass::model::io {

namespace {

using types::value_id;
using types::value_kind;

void write_string(std::string& out, std::string_view s) {
    constexpr char hex[] = "0123456789abcdef";
    out += '"';
    for (const char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out += "\\u00";
                out += hex[(c >> 4) & 0xF];
                out += hex[c & 0xF];
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

void indent(std::string& out, int depth) {
    out += '\n';
    out.append(static_cast<std::size_t>(depth) * 2, ' ');
}

void write_value(std::string& out, const types::model_tables& t, value_id v,
    int depth) {
    const auto& values = t.values;
    switch (values.kinds[v]) {
    case value_kind::null: out += "null"; return;
    case value_kind::boolean:
        out += values.scalars[v] != 0 ? "true" : "false";
        return;
    case value_kind::integer:
        out += std::to_string(values.scalars[v]);
        return;
    case value_kind::string:
        write_string(out, types::text_of(t.texts,
            types::text_id{static_cast<std::uint32_t>(values.scalars[v])}));
        return;
    case value_kind::list:
    case value_kind::object:
        break;
    }
    std::vector<value_id> children;
    for (auto c = values.firsts[v]; !c.is_none(); c = values.nexts[c])
        children.push_back(c);
    const bool object = values.kinds[v] == value_kind::object;
    if (children.empty()) {
        out += object ? "{}" : "[]";
        return;
    }
    if (object) {
        std::ranges::sort(children, [&](value_id a, value_id b) {
            return types::text_of(t.texts, values.keys[a]) <
                types::text_of(t.texts, values.keys[b]);
        });
    }
    out += object ? '{' : '[';
    for (std::size_t i = 0; i < children.size(); ++i) {
        if (i != 0)
            out += ',';
        indent(out, depth + 1);
        if (object) {
            write_string(out, types::text_of(t.texts,
                values.keys[children[i]]));
            out += ": ";
        }
        write_value(out, t, children[i], depth + 1);
    }
    indent(out, depth);
    out += object ? '}' : ']';
}

}

void write_json(std::string& out, const types::model_tables& t,
    value_id value) {
    write_value(out, t, value, 0);
    out += '\n';
}

namespace {

/**
 * @brief A recursive descent over JSON text. Each value is written where
 * its parent puts it: under a key of an object or at the end of a list.
 */
class reader final {
public:
    reader(ops::builder& b, std::string_view text) : b_(b), text_(text) {}

    value_id top() {
        skip_space();
        value_id root;
        if (peek() == '{')
            root = b_.detached_object();
        else if (peek() == '[')
            root = b_.detached_list();
        else
            fail("expected an object or an array");
        fill(root);
        skip_space();
        if (pos_ != text_.size())
            fail("text after the value");
        return root;
    }

private:
    [[noreturn]] void fail(std::string_view what) const {
        throw std::runtime_error("JSON at byte " + std::to_string(pos_) +
            ": " + std::string(what));
    }

    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

    void skip_space() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' ||
            text_[pos_] == '\n' || text_[pos_] == '\r' || text_[pos_] == '\t'))
            ++pos_;
    }

    void expect(char c) {
        skip_space();
        if (peek() != c)
            fail(std::string("expected '") + c + "'");
        ++pos_;
    }

    bool literal(std::string_view word) {
        if (text_.substr(pos_, word.size()) != word)
            return false;
        pos_ += word.size();
        return true;
    }

    unsigned hex4() {
        if (pos_ + 4 > text_.size())
            fail("short \\u escape");
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9')
                v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f')
                v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                v |= static_cast<unsigned>(c - 'A' + 10);
            else
                fail("bad \\u escape");
        }
        return v;
    }

    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    /**
     * @brief The string at the cursor. It points into the text unless the
     * string holds an escape, when it is unescaped into @p buffer.
     */
    std::string_view string(std::string& buffer) {
        expect('"');
        const auto start = pos_;
        while (pos_ < text_.size() && text_[pos_] != '"' &&
            text_[pos_] != '\\')
            ++pos_;
        if (peek() == '"')
            return text_.substr(start, pos_++ - start);
        buffer.assign(text_.substr(start, pos_ - start));
        while (true) {
            if (pos_ >= text_.size())
                fail("unterminated string");
            const char c = text_[pos_++];
            if (c == '"')
                return buffer;
            if (c != '\\') {
                buffer += c;
                continue;
            }
            if (pos_ >= text_.size())
                fail("unterminated string");
            switch (text_[pos_++]) {
            case '"': buffer += '"'; break;
            case '\\': buffer += '\\'; break;
            case '/': buffer += '/'; break;
            case 'b': buffer += '\b'; break;
            case 'f': buffer += '\f'; break;
            case 'n': buffer += '\n'; break;
            case 'r': buffer += '\r'; break;
            case 't': buffer += '\t'; break;
            case 'u': {
                unsigned cp = hex4();
                if (cp >= 0xD800 && cp < 0xDC00 && literal("\\u")) {
                    const unsigned low = hex4();
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                utf8(buffer, cp);
                break;
            }
            default: fail("bad escape");
            }
        }
    }

    /**
     * @brief Reads the value at the cursor into @p key of object @p parent,
     * or at the end of list @p parent when @p key is null.
     */
    void value(value_id parent, const std::string* key) {
        skip_space();
        const char c = peek();
        if (c == '{' || c == '[') {
            const bool object = c == '{';
            value_id child;
            if (key != nullptr)
                child = object ? b_.object(parent, *key) : b_.list(parent, *key);
            else
                child = object ? b_.append_object(parent)
                               : b_.append_list(parent);
            fill(child);
        } else if (c == '"') {
            std::string buffer;
            const auto s = string(buffer);
            if (key != nullptr)
                b_.set(parent, *key, s);
            else
                b_.append(parent, s);
        } else if (literal("true") || literal("false")) {
            const bool v = text_[pos_ - 1] == 'e' && text_[pos_ - 2] == 'u';
            if (key != nullptr)
                b_.set(parent, *key, v);
            else
                b_.append(parent, v);
        } else if (literal("null")) {
            if (key != nullptr)
                b_.set_null(parent, *key);
            else
                b_.append_null(parent);
        } else {
            number(parent, key);
        }
    }

    void number(value_id parent, const std::string* key) {
        const auto start = pos_;
        bool integral = true;
        if (peek() == '-')
            ++pos_;
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c >= '0' && c <= '9') {
                ++pos_;
            } else if (c == '.' || c == 'e' || c == 'E' || c == '+' ||
                c == '-') {
                integral = false;
                ++pos_;
            } else {
                break;
            }
        }
        const auto text = text_.substr(start, pos_ - start);
        if (text.empty() || text == "-")
            fail("bad value");
        if (!integral) {
            if (key != nullptr)
                b_.set(parent, *key, text);
            else
                b_.append(parent, text);
            return;
        }
        const bool negative = text.front() == '-';
        std::int64_t v = 0;
        for (const char c : negative ? text.substr(1) : text)
            v = v * 10 + (c - '0');
        if (negative)
            v = -v;
        if (key != nullptr)
            b_.set_integer(parent, *key, v);
        else
            b_.append_integer(parent, v);
    }

    void fill(value_id container) {
        const bool object = peek() == '{';
        const char close = object ? '}' : ']';
        ++pos_;
        skip_space();
        if (peek() == close) {
            ++pos_;
            return;
        }
        std::string buffer;
        std::string key;
        while (true) {
            if (object) {
                skip_space();
                key = string(buffer);
                expect(':');
                value(container, &key);
            } else {
                value(container, nullptr);
            }
            skip_space();
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            expect(close);
            return;
        }
    }

    ops::builder& b_;
    std::string_view text_;
    std::size_t pos_ = 0;
};

}

value_id read_json(ops::builder& b, std::string_view text) {
    return reader(b, text).top();
}

}
