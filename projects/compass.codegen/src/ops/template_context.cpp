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
#include "compass.codegen/ops/template_context.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "compass.model/ops/typescript.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "entity_surfaces.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::model_tables;
using compass::model::types::value_id;

namespace {

std::string text(const model_tables& t, value_id object, std::string_view key) {
    return values::py_str(t, values::member(t, object, key), "");
}

/*
 * _SHELL_VERB_SENTENCE: what a shell verb asks of the store.
 */
std::string_view verb_sentence(std::string_view verb) {
    constexpr std::pair<std::string_view, std::string_view> sentences[] = {
        {"list", "Reads one page of the collection. Nothing addresses it but "
            "the caller's tenant, so it is the read that shows what exists."},
        {"get", "Reads the one row its key addresses. A key states every "
            "identifying column, so a partial key is refused rather than "
            "answered with an arbitrary row."},
        {"get_many", "Reads several rows in one request, one key group per "
            "row."},
        {"put", "Writes a row."},
        {"put_many", "Writes several rows in one request. The count states "
            "how many field groups follow."},
        {"delete", "Removes a row."},
        {"delete_many", "Removes several rows in one request."},
        {"list_versions", "Reads the row's recorded history, newest first."},
        {"get_version", "Reads one recorded version of the row."},
        {"list_scoped", "Reads one page of the rows that share a relation "
            "value. The relation is part of the address rather than a filter "
            "applied after the read."}};
    for (const auto& [v, sentence] : sentences) {
        if (v == verb)
            return sentence;
    }
    return {};
}

/*
 * _SHELL_PRECONDITION_SENTENCE: what separates two commands of one verb.
 */
std::string_view precondition_sentence(std::string_view precondition) {
    if (precondition == "must_not_exist")
        return "The change states that the row must not exist, so a create "
            "that collides with an existing row is refused rather than "
            "replacing it.";
    if (precondition == "must_match_version")
        return "The change states the version it expects, so it is a "
            "compare-and-swap against the version the caller last read.";
    if (precondition == "any")
        return "The change states no precondition, so it replaces whatever "
            "row the key already addresses.";
    return {};
}

/*
 * _shell_command_commentary for a derived entity command.
 */
std::string command_commentary(const model_tables& t, value_id command) {
    const auto kind = text(t, command, "kind");
    std::vector<std::string_view> sentences{
        values::strip(verb_sentence(text(t, command, "verb")))};
    const auto precondition = text(t, command, "precondition");
    if (kind == "put" || kind == "put_many")
        sentences.push_back(precondition_sentence(precondition));
    if (values::truthy(t, command, "has_intent"))
        sentences.emplace_back("The change carries a reason and a commentary, "
            "which the server records on it so the row's history says why it "
            "moved.");
    if (values::truthy(t, command, "allows_version"))
        sentences.emplace_back("Passing --version makes the write conditional "
            "on the version the caller last read.");
    std::string out;
    for (const auto s : sentences) {
        if (!s.empty())
            out += (out.empty() ? "" : " ") + std::string(s);
    }
    return out;
}

/*
 * shell_recipe_document for a domain entity's shell plan.
 */
void write_shell_recipe(builder& b, value_id context, value_id entity) {
    const auto& t = b.tables();
    const auto component = text(t, entity, "component");
    const auto singular = text(t, entity, "entity_singular");
    const auto plural = text(t, entity, "entity_plural");
    const auto menu = values::truthy(t, entity, "entity_plural") ? plural :
        values::truthy(t, entity, "entity_singular") ? singular : std::string();
    const auto commands = values::items(t,
        values::member(t, entity, "shell"), "commands");

    const auto doc = b.object(context, "shell_recipe");
    b.set(doc, "id", recipe_org_id(component + "." + menu));
    b.set(doc, "component", component);
    b.set(doc, "menu", menu);
    b.set(doc, "singular", singular);
    b.set(doc, "plural", plural);
    b.set(doc, "title", "How do I run the " + menu +
        " commands from the shell?");
    b.set(doc, "description", "Every command the " + menu + " submenu "
        "answers, with the script each one exports into the shell's script "
        "library.");
    const auto what = "the " + plural + " resource";
    if (commands.size() == 1)
        b.set(doc, "intro", "This file documents " + what + " at the shell. "
            "Its one command is =ores-shell> " + menu + " " +
            text(t, commands[0], "command") + "=, and the section below "
            "exports it as a script into =projects/ores.shell/scripts/library/"
            "=, which the shell can load.");
    else
        b.set(doc, "intro", "This file documents " + what + " at the shell, "
            "one section per command. Every command is a subcommand of =" +
            menu + "=, and every section exports its own script into "
            "=projects/ores.shell/scripts/library/=. Loading a script runs "
            "that one command, so a failure names the command it came from.");
    b.set(doc, "is_operation", false);
    const auto list = b.list(doc, "commands");
    for (const auto c : commands) {
        const auto name = text(t, c, "command");
        const auto block = menu + "-" + name;
        const auto o = b.append_object(list);
        b.set(o, "command", name);
        b.set(o, "heading", name);
        b.set(o, "block", block);
        b.set(o, "id", recipe_org_id(component + "." + block));
        b.set(o, "script", block + ".ores");
        b.set(o, "usage", menu + " " + text(t, c, "usage"));
        b.set(o, "invocation", menu + " " + text(t, c, "invocation"));
        b.set(o, "verb", text(t, c, "verb"));
        b.set(o, "subject", text(t, c, "subject"));
        b.set(o, "request", text(t, c, "request"));
        const auto response = values::member(t, c, "response_type");
        if (response.is_none())
            b.set(o, "response_type", "");
        else
            b.set_copy(o, "response_type", response);
        b.set(o, "commentary", command_commentary(t, c));
        b.set(o, "is_destructive", values::truthy(t, c, "is_destructive"));
    }
    b.set_integer(doc, "command_count",
        static_cast<std::int64_t>(commands.size()));
}

/*
 * _http_sample_path: the route's path with a sentinel in every
 * placeholder.
 */
std::string sample_path(const model_tables& t, value_id route) {
    auto pattern = text(t, route, "pattern");
    const auto replace = [&](const std::string& from, const std::string& to) {
        for (auto at = pattern.find(from); at != std::string::npos;
             at = pattern.find(from, at + to.size()))
            pattern.replace(at, from.size(), to);
    };
    for (const auto key : values::items(t, route, "keys")) {
        const auto name = text(t, key, "name");
        if (!name.empty())
            replace("{" + name + "}", surfaces::sentinel_for_field(name,
                text(t, key, "cpp_type")));
    }
    if (text(t, route, "kind") == "version_read")
        replace("{version}",
            surfaces::sentinel_for_field("version", "std::uint32_t"));
    const auto word = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_';
    };
    std::string out;
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i] == '{') {
            auto end = i + 1;
            while (end < pattern.size() && word(pattern[end]))
                ++end;
            if (end > i + 1 && end < pattern.size() && pattern[end] == '}') {
                out += surfaces::sentinel_for_field(
                    pattern.substr(i + 1, end - i - 1), "");
                i = end;
                continue;
            }
        }
        out += pattern[i];
    }
    return out;
}

/*
 * _http_route_commentary: what one endpoint is for.
 */
std::string route_commentary(const model_tables& t, value_id route) {
    const auto summary = std::string(values::strip(text(t, route, "summary")));
    const auto sentence = summary.empty() ? std::string() :
        text::upper(summary.substr(0, 1)) + summary.substr(1) + ".";
    const auto session = values::member(t, route, "requires_session");
    const auto auth = session.is_none() || values::truthy(t, session) ?
        "It needs a session." : "It needs no session.";
    return std::string(values::strip(sentence + " " + auth));
}

/*
 * http_recipe_document for a domain entity's route plan.
 */
void write_http_recipe(builder& b, value_id context, value_id entity) {
    const auto& t = b.tables();
    const auto component = text(t, entity, "component");
    const auto singular = text(t, entity, "entity_singular");
    const auto plural = text(t, entity, "entity_plural");
    const auto routes = values::items(t,
        values::member(t, entity, "http_route"), "routes");

    const auto doc = b.object(context, "http_recipe");
    b.set(doc, "id", recipe_org_id(component + "." + plural));
    b.set(doc, "component", component);
    b.set(doc, "group", plural);
    b.set(doc, "singular", singular);
    b.set(doc, "plural", plural);
    b.set(doc, "title", "How do I call the " + plural +
        " endpoints over HTTP?");
    b.set(doc, "description", "Every endpoint the " + plural + " resource "
        "answers over HTTP, with the request each one exports into the "
        "gateway's recipe library.");
    const auto list = b.list(doc, "endpoints");
    std::string first_heading;
    for (const auto r : routes) {
        const auto name = text(t, r, "command");
        const auto block = plural + "-" + name;
        const auto pattern = sample_path(t, r);
        const auto heading = text(t, r, "method_upper") + " " + pattern;
        if (first_heading.empty())
            first_heading = heading;
        const auto session = values::member(t, r, "requires_session");
        const auto o = b.append_object(list);
        b.set(o, "command", name);
        b.set(o, "heading", heading);
        b.set(o, "block", block);
        b.set(o, "id", recipe_org_id(component + "." + block));
        b.set(o, "name", block);
        b.set(o, "status", "200");
        b.set(o, "method_upper", text(t, r, "method_upper"));
        b.set(o, "pattern", pattern);
        b.set(o, "subject", text(t, r, "subject"));
        b.set(o, "requires_session",
            session.is_none() || values::truthy(t, session));
        b.set(o, "commentary", route_commentary(t, r));
    }
    if (routes.size() == 1)
        b.set(doc, "intro", "This file documents the " + plural +
            " resource over HTTP. Its one endpoint is =" + first_heading +
            "=, and the section below exports it as a Hurl file into "
            "=projects/ores.http/scripts/library/=, which the harness runs.");
    else
        b.set(doc, "intro", "This file documents the " + plural +
            " resource over HTTP, one section per endpoint. Every section "
            "exports its own Hurl file into "
            "=projects/ores.http/scripts/library/=. Running a file sends that "
            "one request, so a failure names the endpoint it came from.");
    b.set_integer(doc, "endpoint_count",
        static_cast<std::int64_t>(routes.size()));
}

/*
 * ts_protocol.ts.mustache's imports: utility_imports and the domain
 * interfaces other than the entity's own, extra_domain_imports.
 */
void write_ts_imports(builder& b, value_id entity,
    const types::entity_registry& registry) {
    const auto& t = b.tables();
    std::vector<std::string_view> cpp_types;
    for (const auto m : values::items(t, entity, "messages")) {
        for (const auto f : values::items(t, m, "fields"))
            cpp_types.push_back(values::str(t, f, "cpp_type"));
    }
    const auto utility = b.list(entity, "utility_imports");
    for (const auto& i : compass::model::ops::ts_utility_imports(cpp_types)) {
        const auto o = b.append_object(utility);
        b.set(o, "name_pascal", i.pascal);
        b.set(o, "module", i.module);
    }
    const auto component = values::member(t, entity, "component");
    const auto owner = values::is(t, component,
        compass::model::types::value_kind::string) ?
        std::optional<std::string_view>(values::str(t, component)) :
        std::nullopt;
    const auto singular = values::str(t, entity, "entity_singular");
    const auto imports = compass::model::ops::ts_domain_imports(cpp_types,
        owner, registry.custom_types);
    const auto extra = b.list(entity, "extra_domain_imports");
    for (const auto& i : imports) {
        if (i.entity == singular)
            continue;
        const auto o = b.append_object(extra);
        b.set(o, "entity", i.entity);
        b.set(o, "entity_pascal", i.entity_pascal);
        b.set(o, "module_dir", i.module_dir);
    }
}

/*
 * _convention_family_row: one family with its one-hot mode flags.
 */
void write_family(builder& b, value_id o, const types::convention_family& f) {
    b.set(o, "key", f.key);
    b.set(o, "entity", f.entity);
    b.set(o, "domain_type", f.domain_type);
    b.set(o, "subject_stem", f.subject_stem);
    b.set(o, "party_scoped", f.party_scoped);
    b.set(o, "write_mode", f.write_mode);
    b.set(o, "is_party", f.write_mode == "party");
    b.set(o, "is_by_id", f.write_mode == "by_id");
    b.set(o, "is_world", f.write_mode == "world");
    b.set(o, "has_wire_codec", f.has_wire_codec);
    b.set(o, "codec_include", f.has_wire_codec ? "ores." + f.component +
        ".api/domain/" + f.entity + "_codec.hpp" : std::string());
}

value_id copy_of(builder& b, value_id context) {
    const auto copy = b.detached_object();
    for (const auto c : values::items(b.tables(), context))
        b.set_copy(copy, std::string(values::key_of(b.tables(), c)), c);
    return copy;
}

bool is_registry_template(std::string_view name) {
    constexpr std::array<std::string_view, 4> names{
        "cpp_convention_family_registry.hpp.mustache",
        "cpp_convention_family_units.hpp.mustache",
        "cpp_convention_family_dispatch.cpp.mustache",
        "ts_convention_family_registry.ts.mustache"};
    return std::ranges::find(names, name) != names.end();
}

/*
 * The #+key: lines before the first heading, keys lowered, as
 * convention_family_registry's _frontmatter reads them.
 */
std::map<std::string, std::string, std::less<>> frontmatter(
    std::string_view text_of) {
    std::map<std::string, std::string, std::less<>> out;
    for (const auto raw : text::split(text_of, '\n')) {
        const auto line = raw.ends_with('\r') ? raw.substr(0, raw.size() - 1) :
            raw;
        if (line.starts_with('*'))
            break;
        if (!line.starts_with("#+"))
            continue;
        const auto colon = line.find(':');
        if (colon == std::string_view::npos || colon == 2)
            continue;
        const auto key = line.substr(2, colon - 2);
        if (!std::ranges::all_of(key, [](char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-';
            }))
            continue;
        out[text::lower(key)] =
            std::string(values::strip(line.substr(colon + 1)));
    }
    return out;
}

}

std::string recipe_org_id(std::string_view name) {
    constexpr std::array<std::uint8_t, 16> space{0x6F, 0x1D, 0x2C, 0x3A, 0x7E,
        0x4B, 0x4C, 0x58, 0x9A, 0x21, 0x8D, 0x3F, 0x5B, 0x7C, 0x0E, 0x64};
    std::string data(space.begin(), space.end());
    data += name;
    auto digest = compass::store::ops::sha1_of(data);
    digest[6] = static_cast<std::uint8_t>((digest[6] & 0x0F) | 0x50);
    digest[8] = static_cast<std::uint8_t>((digest[8] & 0x3F) | 0x80);
    std::string out;
    char buffer[3];
    for (std::size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            out += '-';
        std::snprintf(buffer, sizeof buffer, "%02X", digest[i]);
        out += buffer;
    }
    return out;
}

std::optional<types::convention_family> convention_family_of(
    std::string_view path, std::string_view text_of) {
    const auto front = frontmatter(text_of);
    const auto get = [&](std::string_view key) {
        const auto it = front.find(key);
        return it == front.end() ? std::string() : it->second;
    };
    const auto key = get("convention_family");
    if (key.empty() || get("type") != "ores.codegen.entity")
        return std::nullopt;

    bool party_scoped = false;
    bool in_columns = false;
    std::optional<bool> wire_codec;
    bool before_heading = true;
    for (const auto raw : text::split(text_of, '\n')) {
        const auto line = raw.ends_with('\r') ? raw.substr(0, raw.size() - 1) :
            raw;
        if (line.starts_with('*'))
            before_heading = false;
        if (before_heading) {
            constexpr std::string_view drawer = ":ores.cpp.wire-codec.enabled:";
            const auto stripped = values::strip(line);
            if (!wire_codec && stripped.starts_with(drawer))
                wire_codec = text::lower(values::strip(
                    stripped.substr(drawer.size()))) == "true";
            continue;
        }
        if (line.starts_with("* ")) {
            in_columns = values::strip(line) == "* Columns";
            continue;
        }
        if (in_columns && line.starts_with("** ")) {
            const auto heading = values::strip(line.substr(3));
            party_scoped = party_scoped ||
                heading.substr(0, heading.find_first_of(" \t")) == "party_id";
        }
    }

    const auto file = path.substr(path.rfind('/') + 1);
    const auto stem = file.substr(0, file.rfind('.'));
    const auto stated = get("entity_singular");
    const auto entity = std::string(values::strip(stated.empty() ?
        stem.substr(stem.rfind('.') + 1) : std::string_view(stated)));
    const auto component = get("component");
    const auto stated_plural = get("entity_plural");
    const auto plural = stated_plural.empty() ? entity + "s" : stated_plural;
    const auto declared = get("convention_write");
    return types::convention_family{.key = key, .entity = entity,
        .domain_type = "ores::" + component + "::domain::" + entity,
        .subject_stem = component + ".v1." + plural,
        .party_scoped = party_scoped,
        .write_mode = !declared.empty() ? declared :
            party_scoped ? "party" : "world",
        .component = component, .has_wire_codec = wire_codec.value_or(false)};
}

value_id template_context(builder& b, value_id context,
    std::string_view model_key, std::string_view model_type,
    std::string_view template_name, const types::entity_registry& registry) {
    const auto& t = b.tables();
    if (model_type == "component") {
        if (!is_registry_template(template_name))
            return value_id::none();
        const auto copy = copy_of(b, context);
        const auto o = b.object(copy, "convention_registry");
        b.set(o, "component", "refdata");
        const auto list = b.list(o, "families");
        for (const auto& f : registry.convention_families)
            write_family(b, b.append_object(list), f);
        return copy;
    }
    if (model_type != "domain_entity")
        return value_id::none();
    const auto entity = values::member(t, context, "domain_entity");
    if (template_name == "shell_recipe.org.mustache") {
        const auto copy = copy_of(b, context);
        write_shell_recipe(b, copy, entity);
        return copy;
    }
    if (template_name == "http_recipe.org.mustache") {
        const auto copy = copy_of(b, context);
        write_http_recipe(b, copy, entity);
        return copy;
    }
    if (template_name == "cpp_convention_family_unit.cpp.mustache") {
        const auto singular = values::str(t, entity, "entity_singular");
        const auto family = std::ranges::find(registry.convention_families,
            singular, &types::convention_family::entity);
        if (family == registry.convention_families.end())
            return value_id::none();
        const auto copy = copy_of(b, context);
        write_family(b, b.object(copy, "convention_unit"), *family);
        return copy;
    }
    if (template_name == "ts_protocol.ts.mustache") {
        const auto copy = copy_of(b, context);
        write_ts_imports(b, values::member(t, copy, "domain_entity"), registry);
        write_ts_imports(b, values::member(t,
            values::member(t, copy, model_key), "domain_entity"), registry);
        return copy;
    }
    return value_id::none();
}

}
