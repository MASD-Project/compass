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
#include "compass.cpp/ops/exported_types.hpp"

#include <algorithm>
#include "compass.cpp/ops/text.hpp"

namespace compass::cpp::ops {

namespace {

constexpr bool is_macro_character(char c) noexcept {
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/**
 * @brief True for ORES_ and a run of [A-Z0-9_] ending in @p tail, with
 * at least one character before the tail.
 */
bool is_ores_macro(std::string_view name, std::string_view tail) noexcept {
    return name.starts_with("ORES_") &&
        name.size() >= 5 + tail.size() + 1 && name.ends_with(tail) &&
        std::ranges::all_of(name.substr(5), is_macro_character);
}

bool has_library_guard(const lexed& l) {
    for (const auto d : l.directives) {
        const auto p = parts_of(l, d);
        if (p.keyword != "ifdef")
            continue;
        const auto rest = strip(p.rest);
        auto end = std::size_t{0};
        while (end < rest.size() && is_macro_character(rest[end]))
            ++end;
        const auto name = rest.substr(0, end);
        if (name.starts_with("ORES_") &&
            name.find("_LIBRARY", 6) != std::string_view::npos)
            return true;
    }
    return false;
}

bool attribute_at(std::string_view body, std::size_t i) noexcept {
    if (body.substr(i, 19) == "BOOST_SYMBOL_EXPORT" ||
        body.substr(i, 19) == "BOOST_SYMBOL_IMPORT")
        return true;
    if (body.substr(i, 10) == "__declspec") {
        auto j = skip_spaces(body, i + 10);
        if (j >= body.size() || body[j] != '(')
            return false;
        j = skip_spaces(body, j + 1);
        return body.substr(j, 9) == "dllimport" ||
            body.substr(j, 9) == "dllexport";
    }
    if (body.substr(i, 13) == "__attribute__") {
        auto j = skip_spaces(body, i + 13);
        if (body.substr(j, 2) != "((")
            return false;
        j = skip_spaces(body, j + 2);
        return body.substr(j, 10) == "visibility";
    }
    return false;
}

std::uint32_t line_start(std::string_view text, std::uint32_t offset) {
    const auto newline = text.rfind('\n', offset == 0 ? 0 : offset - 1);
    return offset == 0 || newline == std::string_view::npos ? 0
        : static_cast<std::uint32_t>(newline + 1);
}

}

std::optional<std::string_view> export_macro(const lexed& l,
    const source_model& m) {
    for (const auto& d : m.macros) {
        if (d.function_like || !is_ores_macro(d.name, "_EXPORT"))
            continue;
        if (!strip(d.body).starts_with("BOOST_SYMBOL"))
            continue;
        if (!has_library_guard(l))
            return std::nullopt;
        return d.name;
    }
    return std::nullopt;
}

std::vector<std::string_view> quoted_includes(const source_model& m) {
    std::vector<std::string_view> out;
    for (const auto& i : m.includes) {
        if (!i.angled && !i.target.empty())
            out.push_back(i.target);
    }
    return out;
}

std::vector<exported_type> exported_types(std::string_view source,
    const source_model& m) {
    std::vector<exported_type> out;
    if (source.find("AUTO-GENERATED FILE") != std::string_view::npos)
        return out;
    for (const auto& h : m.heads) {
        if ((h.key == "class" || h.key == "struct") &&
            is_ores_macro(h.macro, "_EXPORT"))
            out.push_back({h.name, line_start(source, h.offset), h.macro});
    }
    return out;
}

bool names_visibility_attribute(std::string_view body) noexcept {
    for (std::size_t i = 0; i < body.size(); ++i) {
        if ((body[i] == 'B' || body[i] == '_') && attribute_at(body, i))
            return true;
    }
    return false;
}

std::vector<std::string_view> visibility_macros(const source_model& m) {
    std::vector<std::string_view> out;
    for (const auto& d : m.macros) {
        if (!d.function_like && names_visibility_attribute(d.body))
            out.push_back(d.name);
    }
    return out;
}

std::vector<enum_site> macro_enum_sites(const lexed& l,
    const source_model& m) {
    const std::string_view code(l.code);
    std::vector<enum_site> out;
    for (const auto& e : m.heads) {
        if (e.key != "enum" || e.macro.empty())
            continue;
        const auto begin = line_start(code, e.offset);
        auto end = code.find('\n', e.offset);
        end = end == std::string_view::npos ? code.size() : end;
        out.push_back({e.offset, e.macro,
            strip(code.substr(begin, end - begin))});
    }
    return out;
}

std::vector<exportable_function> exportable_functions(const lexed& l,
    const source_model& m) {
    const std::string_view code(l.code);
    std::vector<exportable_function> out;
    for (const auto& d : m.declarations) {
        if (d.kind != declaration_kind::function || d.definition ||
            d.is_inline || d.is_constexpr || d.is_consteval ||
            d.is_static || d.is_template || d.qualified || d.anonymous)
            continue;
        // A macro invocation at namespace scope reads as a function with
        // no return type, so a declaration needs one.
        auto decorations = components_of_name(code.substr(d.start,
            d.offset - d.start));
        if (decorations.empty())
            continue;
        out.push_back({line_start(code, d.start), d.name,
            std::move(decorations)});
    }
    return out;
}

}
