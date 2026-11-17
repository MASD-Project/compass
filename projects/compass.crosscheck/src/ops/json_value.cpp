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
#include "compass.crosscheck/ops/json_value.hpp"

#include <cstdint>
#include "compass.crosscheck/ops/python_numbers.hpp"

namespace compass::crosscheck::ops {

namespace {

class parser final {
public:
    explicit parser(std::string_view text) : text_(text) {}

    std::optional<json_value> document() {
        auto v = value();
        skip();
        if (!v || i_ != text_.size())
            return std::nullopt;
        return v;
    }

private:
    void skip() {
        while (i_ < text_.size() && (text_[i_] == ' ' || text_[i_] == '\t' ||
                   text_[i_] == '\n' || text_[i_] == '\r'))
            ++i_;
    }

    bool eat(char c) {
        skip();
        if (i_ < text_.size() && text_[i_] == c) {
            ++i_;
            return true;
        }
        return false;
    }

    bool word(std::string_view w) {
        if (text_.substr(i_, w.size()) != w)
            return false;
        i_ += w.size();
        return true;
    }

    std::optional<json_value> value() {
        skip();
        if (i_ >= text_.size())
            return std::nullopt;
        json_value v;
        switch (text_[i_]) {
        case '{': return object();
        case '[': return list();
        case '"': {
            auto s = string();
            if (!s)
                return std::nullopt;
            return json_value::string(std::move(*s));
        }
        case 't':
            if (!word("true"))
                return std::nullopt;
            return json_value::boolean(true);
        case 'f':
            if (!word("false"))
                return std::nullopt;
            return json_value::boolean(false);
        case 'n':
            if (!word("null"))
                return std::nullopt;
            return v;
        default: return number();
        }
    }

    std::optional<json_value> number() {
        const auto start = i_;
        bool real = false;
        if (i_ < text_.size() && text_[i_] == '-')
            ++i_;
        const auto digits = [&] {
            const auto from = i_;
            while (i_ < text_.size() && text_[i_] >= '0' && text_[i_] <= '9')
                ++i_;
            return i_ > from;
        };
        if (!digits())
            return std::nullopt;
        if (i_ < text_.size() && text_[i_] == '.') {
            ++i_;
            real = true;
            if (!digits())
                return std::nullopt;
        }
        if (i_ < text_.size() && (text_[i_] == 'e' || text_[i_] == 'E')) {
            ++i_;
            real = true;
            if (i_ < text_.size() && (text_[i_] == '+' || text_[i_] == '-'))
                ++i_;
            if (!digits())
                return std::nullopt;
        }
        json_value v;
        v.what = real ? json_value::kind::real : json_value::kind::integer;
        v.text = std::string(text_.substr(start, i_ - start));
        return v;
    }

    static void append_utf8(std::string& out, std::uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    std::optional<std::uint32_t> hex4() {
        if (i_ + 4 > text_.size())
            return std::nullopt;
        std::uint32_t cp = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = text_[i_++];
            cp <<= 4;
            if (c >= '0' && c <= '9')
                cp |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f')
                cp |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                cp |= static_cast<std::uint32_t>(c - 'A' + 10);
            else
                return std::nullopt;
        }
        return cp;
    }

    std::optional<std::string> string() {
        if (!eat('"'))
            return std::nullopt;
        std::string out;
        while (i_ < text_.size()) {
            const char c = text_[i_++];
            if (c == '"')
                return out;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (i_ >= text_.size())
                return std::nullopt;
            switch (const char e = text_[i_++]) {
            case '"': case '\\': case '/': out.push_back(e); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                auto cp = hex4();
                if (!cp)
                    return std::nullopt;
                if (*cp >= 0xD800 && *cp < 0xDC00 &&
                    text_.substr(i_, 2) == "\\u") {
                    const auto save = i_;
                    i_ += 2;
                    const auto low = hex4();
                    if (low && *low >= 0xDC00 && *low < 0xE000)
                        cp = 0x10000 + ((*cp - 0xD800) << 10) + (*low - 0xDC00);
                    else
                        i_ = save;
                }
                append_utf8(out, *cp);
                break;
            }
            default: return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<json_value> list() {
        json_value v;
        v.what = json_value::kind::list;
        eat('[');
        if (eat(']'))
            return v;
        do {
            auto item = value();
            if (!item)
                return std::nullopt;
            v.items.push_back(std::move(*item));
        } while (eat(','));
        if (!eat(']'))
            return std::nullopt;
        return v;
    }

    std::optional<json_value> object() {
        json_value v;
        v.what = json_value::kind::object;
        eat('{');
        if (eat('}'))
            return v;
        do {
            skip();
            auto key = string();
            if (!key || !eat(':'))
                return std::nullopt;
            auto item = value();
            if (!item)
                return std::nullopt;
            bool replaced = false;
            for (std::size_t k = 0; k < v.keys.size(); ++k)
                if (v.keys[k] == *key) {
                    v.items[k] = std::move(*item);
                    replaced = true;
                }
            if (!replaced) {
                v.keys.push_back(std::move(*key));
                v.items.push_back(std::move(*item));
            }
        } while (eat(','));
        if (!eat('}'))
            return std::nullopt;
        return v;
    }

    std::string_view text_;
    std::size_t i_ = 0;
};

}

json_value json_value::string(std::string text) {
    json_value v;
    v.what = kind::string;
    v.text = std::move(text);
    return v;
}

json_value json_value::boolean(bool value) {
    json_value v;
    v.what = kind::boolean;
    v.text = value ? "true" : "false";
    return v;
}

json_value json_value::integer(long long value) {
    json_value v;
    v.what = kind::integer;
    v.text = std::to_string(value);
    return v;
}

const json_value* json_value::get(std::string_view key) const noexcept {
    if (what != kind::object)
        return nullptr;
    for (std::size_t k = 0; k < keys.size(); ++k)
        if (keys[k] == key)
            return &items[k];
    return nullptr;
}

std::string_view json_value::str() const noexcept {
    return what == kind::string ? std::string_view(text) : std::string_view{};
}

std::string_view json_value::str(std::string_view key) const noexcept {
    const auto* v = get(key);
    return v == nullptr ? std::string_view{} : v->str();
}

bool json_value::truthy() const noexcept {
    switch (what) {
    case kind::null: return false;
    case kind::boolean: return text == "true";
    case kind::integer:
    case kind::real: return text.find_first_not_of("-+0.eE") != std::string::npos;
    case kind::string: return !text.empty();
    case kind::list:
    case kind::object: return !items.empty();
    }
    return false;
}

std::optional<json_value> read_json_value(std::string_view text) {
    if (text.starts_with("\xEF\xBB\xBF"))
        return std::nullopt;
    return parser(text).document();
}

std::string python_str(const json_value& v) {
    switch (v.what) {
    case json_value::kind::null: return "None";
    case json_value::kind::boolean: return v.text == "true" ? "True" : "False";
    case json_value::kind::integer: return python_int(v.text).value_or(v.text);
    case json_value::kind::real: return python_float(v.text).value_or(v.text);
    case json_value::kind::string: return v.text;
    case json_value::kind::list:
    case json_value::kind::object: return {};
    }
    return {};
}

std::optional<json_value> json_of(const python_value& v) {
    using pk = python_value::kind;
    json_value out;
    switch (v.what) {
    case pk::string: return json_value::string(v.text);
    case pk::number:
        out.what = v.text.find_first_of(".eE") == std::string::npos
            ? json_value::kind::integer : json_value::kind::real;
        out.text = v.text;
        return out;
    case pk::name:
        if (v.text == "True" || v.text == "False")
            return json_value::boolean(v.text == "True");
        if (v.text == "None")
            return out;
        return std::nullopt;
    case pk::sequence:
        out.what = json_value::kind::list;
        for (const auto& item : v.items) {
            auto j = json_of(item);
            if (!j)
                return std::nullopt;
            out.items.push_back(std::move(*j));
        }
        return out;
    case pk::dict:
        out.what = json_value::kind::object;
        for (const auto& e : v.entries) {
            std::string key = e.key;
            if (!e.key_items.empty()) {
                for (std::size_t k = 0; k < e.key_items.size(); ++k) {
                    const auto part = json_of(e.key_items[k]);
                    if (!part || part->is(json_value::kind::list) ||
                        part->is(json_value::kind::object))
                        return std::nullopt;
                    key += (k == 0 ? "" : "|") + part->text;
                }
            }
            auto j = json_of(e.value);
            if (!j)
                return std::nullopt;
            out.keys.push_back(std::move(key));
            out.items.push_back(std::move(*j));
        }
        return out;
    case pk::format:
    case pk::call: return std::nullopt;
    }
    return std::nullopt;
}

std::optional<json_value> module_table(std::string_view source,
    std::string_view name) {
    const auto v = python_assignment(source, name);
    if (!v)
        return std::nullopt;
    const auto r = resolved(*v, python_string_constants(source));
    if (!r)
        return std::nullopt;
    return json_of(*r);
}

}
