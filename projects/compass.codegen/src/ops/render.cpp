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
#include "compass.codegen/ops/render.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>
#include "compass.codegen/ops/compile.hpp"
#include "compass.model/ops/builder.hpp"

namespace compass::codegen::ops {

namespace {

using compass::model::types::model_tables;
using compass::model::types::text_id;
using compass::model::types::value_id;
using compass::model::types::value_kind;
using types::node_id;
using types::node_kind;

/**
 * @brief ores.codegen's _VALUE_INIT_CPP_TYPES: a member of one of these
 * types is indeterminate unless its struct sets it.
 */
constexpr std::string_view value_init_types[] = {
    "bool", "char", "int", "long", "long long", "short", "unsigned",
    "unsigned int", "unsigned long", "unsigned long long", "double", "float",
    "std::ptrdiff_t", "std::int8_t", "std::int16_t", "std::int32_t",
    "std::int64_t", "std::uint8_t", "std::uint16_t", "std::uint32_t",
    "std::uint64_t", "std::size_t"};

/**
 * @brief The two values cpp_value_init takes: "{}" and "".
 */
struct value_init_values {
    model_tables tables;
    value_id braces;
    value_id empty;

    value_init_values() {
        compass::model::ops::builder b(tables);
        const auto list = b.detached_list();
        b.append(list, std::string_view("{}"));
        b.append(list, std::string_view(""));
        braces = tables.values.firsts[list];
        empty = tables.values.nexts[braces];
    }
};

const value_init_values& value_inits() {
    static const value_init_values v;
    return v;
}

class renderer final {
public:
    explicit renderer(const render_options& o) : options_(o) {}

    void run(std::string& out, const types::compiled_template& t,
        std::span<const value_ref> stack) {
        stack_.assign(stack.begin(), stack.end());
        walk(out, t, node_id{0},
            node_id{static_cast<std::uint32_t>(t.size())});
    }

private:
    static value_kind kind(value_ref v) noexcept {
        return v.tables->values.kinds[v.value];
    }

    static std::string_view text(value_ref v) noexcept {
        return compass::model::types::text_of(v.tables->texts,
            text_id{static_cast<std::uint32_t>(
                v.tables->values.scalars[v.value])});
    }

    static value_ref first(value_ref v) noexcept {
        return {v.tables, v.tables->values.firsts[v.value]};
    }

    static value_ref next(value_ref v) noexcept {
        return {v.tables, v.tables->values.nexts[v.value]};
    }

    static std::string_view key(value_ref v) noexcept {
        return compass::model::types::text_of(v.tables->texts,
            v.tables->values.keys[v.value]);
    }

    static value_ref member(value_ref object, std::string_view name) noexcept {
        for (auto c = first(object); !c.value.is_none(); c = next(c)) {
            if (key(c) == name)
                return c;
        }
        return {};
    }

    value_ref child(value_ref object, std::string_view name) const noexcept {
        if (object.value.is_none() || kind(object) != value_kind::object)
            return {};
        if (options_.value_init && name == "cpp_value_init") {
            const auto type = member(object, "cpp_type");
            if (!type.value.is_none()) {
                const auto& v = value_inits();
                const bool scalar = kind(type) == value_kind::string &&
                    std::ranges::find(value_init_types, text(type)) !=
                        std::end(value_init_types);
                return {&v.tables, scalar ? v.braces : v.empty};
            }
        }
        return member(object, name);
    }

    /**
     * @brief pystache's ContextStack.get: none when nothing holds the name.
     */
    value_ref lookup(std::string_view name) const noexcept {
        if (name == ".")
            return stack_.back();
        auto dot = name.find('.');
        const auto head = name.substr(0, dot);
        value_ref found;
        for (auto it = stack_.rbegin(); it != stack_.rend(); ++it) {
            found = child(*it, head);
            if (!found.value.is_none())
                break;
        }
        while (!found.value.is_none() && dot != std::string_view::npos) {
            const auto end = name.find('.', dot + 1);
            found = child(found, name.substr(dot + 1,
                end == std::string_view::npos ? end : end - dot - 1));
            dot = end;
        }
        return found;
    }

    static bool truthy(value_ref v) noexcept {
        if (v.value.is_none())
            return false;
        switch (kind(v)) {
        case value_kind::null: return false;
        case value_kind::boolean:
        case value_kind::integer:
            return v.tables->values.scalars[v.value] != 0;
        case value_kind::string: return !text(v).empty();
        case value_kind::list:
        case value_kind::object: return !first(v).value.is_none();
        }
        return false;
    }

    static void repr_string(std::string& out, std::string_view s) {
        const bool double_quote = s.find('\'') != std::string_view::npos &&
            s.find('"') == std::string_view::npos;
        const char quote = double_quote ? '"' : '\'';
        constexpr char hex[] = "0123456789abcdef";
        out += quote;
        for (const char c : s) {
            const auto u = static_cast<unsigned char>(c);
            if (c == quote || c == '\\') {
                out += '\\';
                out += c;
            } else if (c == '\n') {
                out += "\\n";
            } else if (c == '\r') {
                out += "\\r";
            } else if (c == '\t') {
                out += "\\t";
            } else if (u < 0x20 || u == 0x7F) {
                out += "\\x";
                out += hex[u >> 4];
                out += hex[u & 0xF];
            } else {
                out += c;
            }
        }
        out += quote;
    }

    /**
     * @brief Python's str() of @p v, or repr() when it is an item of a
     * container.
     */
    static void str(std::string& out, value_ref v, bool repr) {
        switch (kind(v)) {
        case value_kind::null: out += "None"; return;
        case value_kind::boolean:
            out += v.tables->values.scalars[v.value] != 0 ? "True" : "False";
            return;
        case value_kind::integer:
            out += std::to_string(v.tables->values.scalars[v.value]);
            return;
        case value_kind::string:
            if (repr)
                repr_string(out, text(v));
            else
                out += text(v);
            return;
        case value_kind::list:
        case value_kind::object:
            break;
        }
        const bool object = kind(v) == value_kind::object;
        out += object ? '{' : '[';
        for (auto c = first(v); !c.value.is_none(); c = next(c)) {
            if (c.value != first(v).value)
                out += ", ";
            if (object) {
                repr_string(out, key(c));
                out += ": ";
            }
            str(out, c, true);
        }
        out += object ? '}' : ']';
    }

    /**
     * @brief Why @p name, which the options require, has no value: the
     * object it was looked up from, by its name and C++ type when it has
     * them.
     */
    std::string missing(std::string_view name) const {
        std::string out = "nothing holds " + std::string(name);
        const auto top = stack_.back();
        if (top.value.is_none() || kind(top) != value_kind::object)
            return out;
        const auto text_of = [&](std::string_view key) {
            const auto v = member(top, key);
            return v.value.is_none() || kind(v) != value_kind::string ?
                std::string_view{} : text(v);
        };
        auto field = text_of("name");
        if (field.empty())
            field = text_of("column");
        if (!field.empty())
            out += " for " + std::string(field);
        if (const auto type = text_of("cpp_type"); !type.empty())
            out += " of C++ type " + std::string(type);
        return out;
    }

    void interpolate(std::string& out, std::string_view name, bool escape) {
        const auto v = lookup(name);
        if (v.value.is_none()) {
            if (std::ranges::find(options_.required, name) !=
                options_.required.end())
                throw std::runtime_error(missing(name));
            return;
        }
        if (!escape || options_.escape == escaping::none) {
            str(out, v, false);
            return;
        }
        scratch_.clear();
        str(scratch_, v, false);
        for (const char c : scratch_) {
            switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#x27;"; break;
            default: out += c;
            }
        }
    }

    /**
     * @brief pystache indents every line of the partial that is not empty,
     * then parses it with the default delimiters.
     */
    void partial(std::string& out, std::string_view name,
        std::string_view indent) {
        if (!options_.partials)
            return;
        const auto source = options_.partials(name);
        if (!source)
            return;
        std::string indented;
        bool line_start = true;
        for (const char c : *source) {
            if (line_start && c != '\n')
                indented += indent;
            indented += c;
            line_start = c == '\n';
        }
        const auto t = compile(std::move(indented));
        walk(out, t, node_id{0},
            node_id{static_cast<std::uint32_t>(t.size())});
    }

    void walk(std::string& out, const types::compiled_template& t,
        node_id from, node_id to) {
        for (auto n = from; n < to; n = t.ends[n]) {
            switch (t.kinds[n]) {
            case node_kind::text:
                out += t.text(n);
                break;
            case node_kind::escaped:
                interpolate(out, t.text(n), true);
                break;
            case node_kind::literal:
                interpolate(out, t.text(n), false);
                break;
            case node_kind::partial:
                partial(out, t.text(n), t.indent(n));
                break;
            case node_kind::inverted:
                if (!truthy(lookup(t.text(n))))
                    walk(out, t, n.next(), t.ends[n]);
                break;
            case node_kind::section: {
                const auto v = lookup(t.text(n));
                if (!truthy(v))
                    break;
                if (kind(v) != value_kind::list) {
                    stack_.push_back(v);
                    walk(out, t, n.next(), t.ends[n]);
                    stack_.pop_back();
                    break;
                }
                for (auto c = first(v); !c.value.is_none(); c = next(c)) {
                    stack_.push_back(c);
                    walk(out, t, n.next(), t.ends[n]);
                    stack_.pop_back();
                }
                break;
            }
            }
        }
    }

    const render_options& options_;
    std::vector<value_ref> stack_;
    std::string scratch_;
};

}

void render(std::string& out, const types::compiled_template& t,
    std::span<const value_ref> stack, const render_options& options) {
    renderer(options).run(out, t, stack);
}

void render(std::string& out, const types::compiled_template& t,
    const model_tables& values, value_id context,
    const render_options& options) {
    const value_ref base{&values, context};
    render(out, t, std::span(&base, 1), options);
}

}
