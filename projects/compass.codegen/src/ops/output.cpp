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
#include "compass.codegen/ops/output.hpp"

#include <algorithm>
#include <cctype>
#include <vector>
#include "compass.codegen/ops/render.hpp"
#include "compass.model/ops/builder.hpp"

namespace compass::codegen::ops {

namespace {

using compass::model::types::model_tables;
using compass::model::types::text_id;
using compass::model::types::value_id;
using compass::model::types::value_kind;

constexpr std::string_view value_init_templates[] = {
    "cpp_protocol.hpp.mustache", "cpp_field_group.hpp.mustache",
    "cpp_domain_type_class.hpp.mustache",
    "cpp_domain_type_entity.hpp.mustache"};

value_id member(const model_tables& t, value_id object, std::string_view key) {
    if (object.is_none() || t.values.kinds[object] != value_kind::object)
        return value_id::none();
    for (auto c = t.values.firsts[object]; !c.is_none();
         c = t.values.nexts[c]) {
        if (compass::model::types::text_of(t.texts, t.values.keys[c]) == key)
            return c;
    }
    return value_id::none();
}

std::string_view string_of(const model_tables& t, value_id v) {
    return compass::model::types::text_of(t.texts,
        text_id{static_cast<std::uint32_t>(t.values.scalars[v])});
}

bool truthy(const model_tables& t, value_id v) {
    if (v.is_none())
        return false;
    switch (t.values.kinds[v]) {
    case value_kind::null: return false;
    case value_kind::boolean:
    case value_kind::integer: return t.values.scalars[v] != 0;
    case value_kind::string: return !string_of(t, v).empty();
    case value_kind::list:
    case value_kind::object: return !t.values.firsts[v].is_none();
    }
    return false;
}

constexpr std::string_view template_label = "Template: ";
constexpr std::string_view template_suffix = ".mustache";

std::string generated_marker(std::string_view template_name, bool cmake) {
    const std::string lines[] = {
        "AUTO-GENERATED FILE - DO NOT EDIT MANUALLY",
        std::string(template_label) + std::string(template_name),
        "To modify, update the template and regenerate."};
    std::string out;
    if (cmake) {
        for (const auto& line : lines)
            out += (out.empty() ? "# " : "\n# ") + line;
        return out;
    }
    out = "/**";
    for (const auto& line : lines)
        out += "\n * " + line;
    return out + "\n */";
}

/**
 * @brief The licences to shadow for this template: each key whose
 * language the template emits, with the marker after its text.
 */
void add_markers(compass::model::ops::builder& b, value_id overlay,
    const model_tables& values, value_id context,
    std::string_view template_name) {
    struct licence {
        std::string_view key;
        bool emits;
        bool cmake;
    };
    const licence licences[] = {
        {"cpp_license", template_name.ends_with(".hpp.mustache") ||
            template_name.ends_with(".cpp.mustache") ||
            template_name.ends_with(".inc.mustache"), false},
        {"ts_license", template_name.ends_with(".ts.mustache"), false},
        {"cmake_license", template_name.starts_with("cmake_"), true}};
    for (const auto& l : licences) {
        if (!l.emits)
            continue;
        const auto v = member(values, context, l.key);
        if (v.is_none() || values.values.kinds[v] != value_kind::string)
            continue;
        b.set(overlay, l.key, std::string(string_of(values, v)) + "\n" +
            generated_marker(template_name, l.cmake));
    }
}

/**
 * @brief The length of the line break Python's str.splitlines() sees at
 * @p i, or 0.
 */
std::size_t line_break(std::string_view s, std::size_t i) {
    const auto c = static_cast<unsigned char>(s[i]);
    if (c == '\r')
        return i + 1 < s.size() && s[i + 1] == '\n' ? 2 : 1;
    if (c == '\n' || c == 0x0B || c == 0x0C || c == 0x1C || c == 0x1D ||
        c == 0x1E)
        return 1;
    const auto rest = s.substr(i);
    if (rest.starts_with("\xC2\x85"))
        return 2;
    if (rest.starts_with("\xE2\x80\xA8") || rest.starts_with("\xE2\x80\xA9"))
        return 3;
    return 0;
}

std::string_view strip(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

/**
 * @brief _insert_org_marker: the lines joined by \n with the marker after
 * the first :END: line, and a final \n when the text ended with one.
 */
std::string insert_org_marker(std::string_view rendered,
    std::string_view template_name) {
    const std::string marker =
        "#+generated: AUTO-GENERATED FILE - DO NOT EDIT MANUALLY. " +
        std::string(template_label) + std::string(template_name) +
        ". To modify, update the template and regenerate.";
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i < rendered.size();) {
        if (const auto n = line_break(rendered, i)) {
            lines.push_back(rendered.substr(start, i - start));
            i += n;
            start = i;
        } else {
            ++i;
        }
    }
    if (start < rendered.size())
        lines.push_back(rendered.substr(start));
    const auto end = std::ranges::find_if(lines,
        [](std::string_view l) { return strip(l) == ":END:"; });
    if (end == lines.end())
        return marker + "\n" + std::string(rendered);
    std::string out;
    for (auto it = lines.begin(); it != lines.end(); ++it) {
        if (it != lines.begin())
            out += '\n';
        out += *it;
        if (it == end)
            out += "\n" + marker;
    }
    if (rendered.ends_with('\n'))
        out += '\n';
    return out;
}

bool is_uuid_char(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
        (c >= 'A' && c <= 'F') || c == '-';
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

/**
 * @brief The implementations object of the model being rendered: that of
 * the first of domain_entity, junction, component and field_group that is
 * true.
 */
value_id implementations_of(const model_tables& t, value_id context) {
    for (const auto key : {"domain_entity", "junction", "component",
             "field_group"}) {
        const auto owner = member(t, context, key);
        if (truthy(t, owner))
            return member(t, owner, "implementations");
    }
    return value_id::none();
}

/**
 * @brief _substitute_paste_markers: [ \t]*<<paste:UUID>>[ \t]*\n? becomes
 * the blocks that implement the kind, each without its trailing
 * whitespace, joined by a blank line and ended by a line end.
 */
std::string substitute_paste_markers(std::string_view text,
    const model_tables& t, value_id context) {
    constexpr std::string_view open = "<<paste:";
    auto marker = text.find(open);
    if (marker == std::string_view::npos)
        return std::string(text);
    const auto impls = implementations_of(t, context);
    std::string out;
    std::size_t copied = 0;
    for (; marker != std::string_view::npos; marker = text.find(open, marker + 1)) {
        auto id_end = marker + open.size();
        while (id_end < text.size() && is_uuid_char(text[id_end]))
            ++id_end;
        if (id_end == marker + open.size() ||
            text.substr(id_end, 2) != ">>")
            continue;
        auto start = marker;
        while (start > copied && (text[start - 1] == ' ' ||
            text[start - 1] == '\t'))
            --start;
        auto end = id_end + 2;
        while (end < text.size() && (text[end] == ' ' || text[end] == '\t'))
            ++end;
        if (end < text.size() && text[end] == '\n')
            ++end;
        out.append(text.substr(copied, start - copied));
        copied = end;
        const auto id_first = marker + open.size();
        marker = end - 1;

        const auto kind = lower(text.substr(id_first, id_end - id_first));
        value_id blocks;
        if (!impls.is_none() && t.values.kinds[impls] == value_kind::object) {
            for (auto c = t.values.firsts[impls]; !c.is_none();
                 c = t.values.nexts[c]) {
                if (lower(compass::model::types::text_of(t.texts,
                        t.values.keys[c])) == kind)
                    blocks = c;
            }
        }
        if (!truthy(t, blocks) || t.values.kinds[blocks] != value_kind::list)
            continue;
        for (auto b = t.values.firsts[blocks]; !b.is_none();
             b = t.values.nexts[b]) {
            if (b != t.values.firsts[blocks])
                out += "\n\n";
            auto body = string_of(t, b);
            while (!body.empty() &&
                std::isspace(static_cast<unsigned char>(body.back())))
                body.remove_suffix(1);
            out += body;
        }
        out += '\n';
    }
    out.append(text.substr(copied));
    return out;
}

}

void render_output(std::string& out, std::string_view template_name,
    const types::compiled_template& t, const model_tables& values,
    value_id context) {
    model_tables overlay_tables;
    compass::model::ops::builder b(overlay_tables);
    const auto overlay = b.detached_object();
    add_markers(b, overlay, values, context, template_name);

    std::vector<value_ref> stack{{&values, context}};
    if (!overlay_tables.values.firsts[overlay].is_none())
        stack.push_back({&overlay_tables, overlay});

    render_options options;
    options.value_init = std::ranges::find(value_init_templates,
        template_name) != std::end(value_init_templates);

    std::string rendered;
    render(rendered, t, stack, options);
    if (template_name.ends_with(".org.mustache"))
        rendered = insert_org_marker(rendered, template_name);
    out += substitute_paste_markers(rendered, values, context);
}

std::string_view marked_template(std::string_view content) {
    const auto head = content.substr(0, marker_window);
    const auto label = head.find(template_label);
    if (label == std::string_view::npos)
        return {};
    const auto name = head.substr(label + template_label.size());
    const auto end = name.find(template_suffix);
    if (end == std::string_view::npos ||
        name.substr(0, end).find('\n') != std::string_view::npos)
        return {};
    return name.substr(0, end + template_suffix.size());
}

}
