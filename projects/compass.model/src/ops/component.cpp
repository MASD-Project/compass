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
#include <string>
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

/**
 * @brief _option_flag: t, true or yes is true, nothing, nil, false or no is
 * false, and anything else is refused.
 */
bool option_flag(std::string_view owner, std::string_view option,
    std::string_view raw, std::string_view key) {
    const auto value = to_lower(trim(raw));
    if (value.empty() || value == "nil" || value == "false" || value == "no")
        return false;
    if (value == "t" || value == "true" || value == "yes")
        return true;
    throw model_error(std::string(owner) + ": option '" + std::string(option) +
        "' sets :" + std::string(key) + ": to " + value);
}

std::string cpp_string(std::string_view text) {
    std::string out;
    for (const char c : text) {
        if (c == '\\' || c == '"')
            out += '\\';
        out += c;
    }
    return out;
}

/**
 * @brief The value of @p key among @p props with their keys lowered, the
 * last one written winning.
 */
std::string_view lowered(const std::vector<key_value>& props,
    std::string_view key) {
    std::string_view out;
    for (const auto& p : props) {
        if (iequals(p.key, key))
            out = p.value;
    }
    return out;
}

void service_options(load_context& c, value_id component) {
    auto& b = c.b;
    const auto& o = c.o;
    const auto owner = file_name(c.d);
    const auto options = section(o, node_id{0}, "Options");
    const auto nodes = children(o, options);
    if (nodes.empty()) {
        b.set(component, "has_service_options", false);
        b.list(component, "service_options");
        return;
    }
    const auto group = lowered(properties(o, options), "group");
    if (group.empty())
        throw model_error(std::string(owner) + ": * Options needs a :group:");
    const auto list = b.list(component, "service_options");
    std::size_t secrets = 0;
    bool positive_any = false;
    for (const auto n : nodes) {
        const auto props = properties(o, n);
        const auto title = o.titles[n.value];
        for (const auto key : {"cpp_type", "default", "help"}) {
            if (lowered(props, key).empty())
                throw model_error(std::string(owner) + ": option '" +
                    std::string(title) + "' needs " + key);
        }
        const auto item = b.append_object(list);
        b.set(item, "name", title);
        b.set(item, "cpp_type", lowered(props, "cpp_type"));
        const auto option = lowered(props, "option");
        if (option.empty()) {
            std::string dashed(title);
            for (auto& ch : dashed) {
                if (ch == '_')
                    ch = '-';
            }
            b.set(item, "option", dashed);
        } else {
            b.set(item, "option", option);
        }
        b.set(item, "default", lowered(props, "default"));
        b.set(item, "initial", lowered(props, "initial"));
        b.set(item, "help",
            cpp_string(collapse_spaces(lowered(props, "help"))));
        const bool secret =
            option_flag(owner, title, lowered(props, "secret"), "secret");
        const bool positive =
            option_flag(owner, title, lowered(props, "positive"), "positive");
        b.set(item, "secret", secret);
        b.set(item, "positive", positive);
        secrets += secret ? 1 : 0;
        positive_any = positive_any || positive;
        const auto doc = b.list(item, "doc_lines");
        const auto text = body(o, n);
        std::vector<std::string_view> lines;
        std::vector<std::uint32_t> starts;
        split_lines(text, lines, starts);
        if (lines.empty())
            lines.push_back(title);
        for (std::size_t i = 0; i < lines.size(); ++i) {
            std::string line(lines[i]);
            while (!line.empty() && is_space(line.back()))
                line.pop_back();
            b.append(doc, i == 0 ? "@brief " + line : line);
        }
    }
    b.set(component, "has_service_options", true);
    b.set(component, "service_options_group", cpp_string(group));
    b.set_integer(component, "service_secret_count",
        static_cast<std::int64_t>(secrets));
    b.set(component, "has_service_secrets", secrets != 0);
    b.set(component, "has_positive_options", positive_any);
}

/**
 * @brief The first block named @p name in a pre-order walk whose code is
 * not empty, as the overview loader's named_block finds it.
 */
std::string_view named_block_anywhere(const outline& o, std::string_view name) {
    for (std::uint32_t n = 0; n < o.size(); ++n) {
        const auto r = named_block(o, node_id{n}, name);
        if (r.found && !r.value.empty())
            return r.value;
    }
    return {};
}

}

void load_component(load_context& c, value_id root) {
    auto& b = c.b;
    const auto component = b.object(root, "component");
    for (const auto key : {"name", "full_name", "brief"}) {
        const auto r = frontmatter(c.o, key);
        if (r.found)
            b.set(component, key, r.value);
    }
    const auto text = body(c.o, node_id{0});
    if (!text.empty())
        b.set(component, "description", text);
}

void load_component_overview(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    const auto component = b.object(root, "component");
    for (const auto key : {"name", "full_name", "brief"}) {
        const auto r = frontmatter(o, key);
        if (r.found)
            b.set(component, key, r.value);
    }
    const auto description = frontmatter_or(o, "description");
    if (!description.empty())
        b.set(component, "description", collapse_spaces(description));
    b.set(component, "kind", frontmatter_or(o, "component_kind", "flat"));
    const auto parts = b.list(component, "parts");
    std::vector<std::string_view> names;
    std::string_view rest = frontmatter_or(o, "parts");
    for (;;) {
        rest = trim(rest);
        if (rest.empty())
            break;
        std::size_t end = 0;
        while (end < rest.size() && !is_space(rest[end]))
            ++end;
        names.push_back(rest.substr(0, end));
        rest.remove_prefix(end);
    }
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto part = b.append_object(parts);
        b.set(part, "part", names[i]);
        b.set(part, "last", i + 1 == names.size());
    }
    b.set(component, "extra_dependencies",
        named_block_anywhere(o, "extra_dependencies"));
    b.set(component, "extra_test_dependencies",
        named_block_anywhere(o, "extra_test_dependencies"));
    b.set(component, "header_only", truthy(frontmatter_or(o, "header_only")));
    b.set(component, "win32_static_library",
        truthy(frontmatter_or(o, "win32_static_library")));
    b.set(component, "hidden_visibility",
        truthy(frontmatter_or(o, "hidden_visibility")));
    b.set(component, "has_test_fixture",
        to_lower(trim(frontmatter_or(o, "test_fixture"))) != "none");
    collect_implementations(c, component, "implementations");
    service_options(c, component);
}

}
