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
#include "compass.codegen/ops/context.hpp"

#include <cctype>
#include <charconv>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>
#include "compass.codegen/ops/values.hpp"

namespace compass::codegen::ops {

namespace {

using compass::model::types::model_tables;
using compass::model::types::text_id;
using compass::model::types::value_id;
using compass::model::types::value_kind;

constexpr std::string_view run_body_paste_kind =
    "2cd994c9-83ff-4c41-8909-5ad2bf3deccc";
constexpr std::string_view config_decl_paste_kind =
    "f98cc722-3e53-4b86-90e7-69ec5d25702e";

bool is_space(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string_view strip(std::string_view s) {
    while (!s.empty() && is_space(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && is_space(s.back()))
        s.remove_suffix(1);
    return s;
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::vector<std::string_view> split(std::string_view s, char sep) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (true) {
        const auto end = s.find(sep, start);
        out.push_back(s.substr(start, end == std::string_view::npos
            ? std::string_view::npos : end - start));
        if (end == std::string_view::npos)
            return out;
        start = end + 1;
    }
}

std::string join(const std::vector<std::string_view>& parts, std::size_t n) {
    std::string out;
    for (std::size_t i = 0; i < n; ++i)
        out += (i ? "." : "") + std::string(parts[i]);
    return out;
}

std::string replace_dots(std::string_view s, std::string_view with) {
    std::string out;
    for (const char c : s) {
        if (c == '.')
            out += with;
        else
            out += c;
    }
    return out;
}

std::string upper(std::string s) {
    for (auto& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

/**
 * @brief _load_modelines_from_org: each *** <title> :modeline: heading,
 * named after the last dot of its title, takes the first
 * :masd.codec.value: after it.
 */
std::map<std::string, std::string> modelines(std::string_view org) {
    std::map<std::string, std::string> out;
    std::optional<std::string> current;
    for (auto line : split(org, '\n')) {
        if (line.ends_with('\r'))
            line.remove_suffix(1);
        if (line.starts_with("***") && line.size() > 3 && is_space(line[3]) &&
            strip(line).ends_with(":modeline:")) {
            auto title = strip(line.substr(3));
            title = strip(title.substr(0, title.size() - 10));
            const auto dot = title.rfind('.');
            if (dot != std::string_view::npos && dot + 1 < title.size() &&
                title.find_first_of(" \t") == std::string_view::npos) {
                current = std::string(title.substr(dot + 1));
                continue;
            }
        }
        if (!current)
            continue;
        constexpr std::string_view key = ":masd.codec.value:";
        const auto s = strip(line);
        if (s.starts_with(key) && s.size() > key.size() &&
            is_space(s[key.size()])) {
            const auto value = strip(s.substr(key.size()));
            if (!value.empty()) {
                out[*current] = std::string(value);
                current.reset();
            }
        }
    }
    return out;
}

struct comment_format {
    std::string_view prefix;
    std::string_view start;
    std::string_view end;
};

/**
 * @brief generate_license_with_header: the modeline on the opening line,
 * then the copyright and the licence, a line each.
 */
std::string licence_block(std::string_view licence, std::string_view modeline,
    const comment_format& f, int year) {
    std::vector<std::string> lines{"", "Copyright (C) " +
        std::to_string(year) + " Marco Craveiro <marco.craveiro@gmail.com>",
        ""};
    for (const auto l : split(licence, '\n'))
        lines.emplace_back(l);
    std::string out = std::string(f.start) + " -*- " + std::string(modeline) +
        " -*-\n";
    auto bare = f.prefix;
    while (!bare.empty() && is_space(bare.back()))
        bare.remove_suffix(1);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i)
            out += '\n';
        if (strip(lines[i]).empty())
            out += bare;
        else
            out += std::string(f.prefix) + lines[i];
    }
    if (!f.end.empty())
        out += "\n" + std::string(f.end);
    return out;
}

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

std::optional<std::string> string_member(const model_tables& t,
    value_id object, std::string_view key) {
    const auto v = member(t, object, key);
    if (v.is_none() || t.values.kinds[v] != value_kind::string)
        return std::nullopt;
    return std::string(compass::model::types::text_of(t.texts,
        text_id{static_cast<std::uint32_t>(t.values.scalars[v])}));
}

/**
 * @brief The names generate_from_model derives for a component: upper-case
 * and namespace forms, base name and segment, the names of its api, core
 * and service siblings, and whether it pastes the service app's run body
 * or options declaration.
 */
void derive_component(compass::model::ops::builder& b, value_id component) {
    const auto& t = b.tables();
    const auto name = string_member(t, component, "name").value_or("unknown");
    const auto full_name = string_member(t, component, "full_name")
        .value_or("ores." + name);
    b.set(component, "full_name_upper", upper(replace_dots(full_name, "_")));
    b.set(component, "name_upper", upper(replace_dots(name, "_")));
    b.set(component, "namespace", replace_dots(full_name, "::"));
    const auto name_parts = split(name, '.');
    const auto base_name = name_parts.size() > 1
        ? join(name_parts, name_parts.size() - 1) : std::string(name_parts[0]);
    b.set(component, "base_name", base_name);
    b.set(component, "segment", name_parts.size() > 1
        ? std::string(name_parts.back()) : std::string());
    const auto full_parts = split(full_name, '.');
    const auto base_full = full_parts.size() > 1 && name_parts.size() > 1
        ? join(full_parts, full_parts.size() - 1) : "ores." + base_name;
    b.set(component, "api_full_name", base_full + ".api");
    b.set(component, "core_full_name", base_full + ".core");
    b.set(component, "service_full_name", base_full + ".service");

    bool run_body = false;
    bool config_decl = false;
    const auto impls = member(t, component, "implementations");
    if (!impls.is_none() && t.values.kinds[impls] == value_kind::object) {
        for (auto c = t.values.firsts[impls]; !c.is_none();
             c = t.values.nexts[c]) {
            const auto key = lower(compass::model::types::text_of(t.texts,
                t.values.keys[c]));
            run_body = run_body || key == run_body_paste_kind;
            config_decl = config_decl || key == config_decl_paste_kind;
        }
    }
    b.set(component, "has_app_run_body", run_body);
    b.set(component, "has_app_config_decl", config_decl);
}

/**
 * @brief @p s as the body of a single-quoted SQL string literal, each
 * quote doubled, as _sql_literal gives it.
 */
std::string sql_literal(std::string_view s) {
    std::string out;
    for (const char c : s) {
        out += c;
        if (c == '\'')
            out += c;
    }
    return out;
}

/**
 * @brief What generate_from_model derives for a service registry: on each
 * service its SQL-quoted summary and display name, image code, account
 * variable and component, and last on the last service; and, under
 * service_registry_db_services of @p context, the services with a psql
 * variable, last marked on the last of them.
 */
void derive_service_registry(compass::model::ops::builder& b,
    value_id context, value_id registry) {
    const auto services = values::items(b.tables(),
        member(b.tables(), registry, "services"));
    const auto db = b.list(context, "service_registry_db_services");
    value_id last_db = value_id::none();
    for (const auto s : services) {
        const auto& t = b.tables();
        const auto psql_var = string_member(t, s, "psql_var").value_or("");
        const auto name = string_member(t, s, "name").value_or("");
        const auto account_var = string_member(t, s, "account_var");
        b.set(s, "summary_sql",
            sql_literal(string_member(t, s, "summary").value_or("")));
        b.set(s, "display_name_sql",
            sql_literal(string_member(t, s, "display_name").value_or("")));
        b.set(s, "image_code", replace_dots(name, "_"));
        if ((!account_var || account_var->empty()) && !psql_var.empty())
            b.set(s, "account_var", psql_var + "_user");
        constexpr std::string_view suffix = "_service";
        if (!psql_var.empty())
            b.set(s, "component", psql_var.ends_with(suffix)
                ? psql_var.substr(0, psql_var.size() - suffix.size())
                : psql_var);
        if (!psql_var.empty())
            last_db = b.append_copy(db, s);
    }
    if (!last_db.is_none())
        b.set(last_db, "last", true);
    if (!services.empty())
        b.set(services.back(), "last", true);
}

}

int copyright_year(std::string_view codec, std::string_view option) {
    if (option.empty())
        throw std::runtime_error(std::string(codec) + ": set the option "
            "year, the copyright year of the licence blocks");
    int year = 0;
    const auto end = option.data() + option.size();
    const auto [at, error] = std::from_chars(option.data(), end, year);
    if (error != std::errc{} || at != end || year < 1)
        throw std::runtime_error(std::string(codec) + ": the option year is "
            "not a year: " + std::string(option));
    return year;
}

value_id library_values(compass::model::ops::builder& b,
    std::string_view licence, std::string_view modeline_org, int year) {
    const auto root = b.detached_object();
    b.set(root, "licence-GPL-v3", licence);
    const auto lines = modelines(modeline_org);
    const auto object = b.object(root, "modelines");
    for (const auto& [name, value] : lines)
        b.set(object, name, value);

    const auto modeline_of = [&](const std::string& name) {
        const auto it = lines.find(name);
        return it == lines.end() ? std::string() : it->second;
    };
    struct language {
        std::string_view key;
        std::string modeline_name;
        comment_format format;
        bool always;
    };
    const language languages[] = {
        {"sql", "sql", {" * ", "/*", " */"}, true},
        {"cpp", "c++", {" * ", "/*", " */"}, true},
        {"cmake", "cmake", {"# ", "#", ""}, false},
        {"ts", "typescript", {" * ", "/**", " */"}, false}};
    for (const auto& l : languages) {
        const auto modeline = modeline_of(l.modeline_name);
        if (!l.always && modeline.empty())
            continue;
        b.set(root, std::string(l.key) + "_license",
            licence_block(licence, modeline, l.format, year));
        b.set(root, std::string(l.key) + "_modeline", modeline);
    }
    return root;
}

value_id model_context(compass::model::ops::builder& b, value_id library,
    value_id model, std::string_view model_key, std::string_view model_type) {
    const auto context = b.detached_object();
    for (auto c = b.tables().values.firsts[library]; !c.is_none();
         c = b.tables().values.nexts[c]) {
        const std::string key(compass::model::types::text_of(b.tables().texts,
            b.tables().values.keys[c]));
        b.set_copy(context, key, c);
    }
    b.set_copy(context, model_key, model);
    const auto copied = b.find(context, model_key);
    const auto owned = member(b.tables(), copied, model_type);
    if (owned.is_none() ||
        b.tables().values.kinds[owned] != value_kind::object)
        return context;
    if (model_type == "component") {
        derive_component(b, owned);
        b.set_copy(context, "component", owned);
    } else if (model_type == "asset_class_catalogue" ||
        model_type == "outcome_catalogue") {
        b.set_copy(context, model_type, owned);
    } else if (model_type == "service_registry") {
        derive_service_registry(b, context, owned);
        b.set_copy(context, model_type, owned);
    }
    return context;
}

}
