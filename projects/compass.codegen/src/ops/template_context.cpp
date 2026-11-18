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
#include <stdexcept>
#include <vector>
#include "compass.codegen/ops/values.hpp"
#include "compass.model/ops/typescript.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "entity_surfaces.hpp"
#include "entity_text.hpp"
#include "model_steps.hpp"

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
 * _declared_command_commentary: what a declared operation's command sends
 * and prints, what it reads and whether it needs a session.
 */
std::string declared_commentary(const model_tables& t, value_id command) {
    auto out = "Sends =" + text(t, command, "request") + "= and prints =" +
        text(t, command, "response_type") + "=, so the shape is the "
        "protocol's and the meaning is the service's.";
    const auto positionals = values::items(t, command, "positionals");
    if (!positionals.empty()) {
        out += " It reads ";
        for (std::size_t i = 0; i < positionals.size(); ++i)
            out += (i == 0 ? "=" : ", =") + text(t, positionals[i], "name") +
                "=";
        out += ", in that order.";
    }
    if (values::truthy(t, command, "public"))
        out += " A caller runs it before it has a session, so the command "
            "presents no token and refuses none.";
    else
        out += " The caller must have established a session first.";
    if (values::truthy(t, command, "is_destructive"))
        out += " This command destroys the system it runs against. Do not "
            "replay it against an environment you need.";
    return out;
}

/*
 * _shell_command_commentary: a derived entity command's, by its verb and
 * precondition, or a declared operation's.
 */
std::string command_commentary(const model_tables& t, value_id command) {
    if (!values::truthy(t, command, "kind"))
        return declared_commentary(t, command);
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
 * The names a recipe document is filed and titled by.
 */
struct recipe_names {
    std::string component;
    std::string menu;
    std::string singular;
    std::string plural;
};

/*
 * shell_recipe_document: one section per command of @p commands.
 */
void write_shell_recipe(builder& b, value_id context, const recipe_names& n,
    const std::vector<value_id>& commands, bool is_operation) {
    const auto& t = b.tables();
    const auto& [component, menu, singular, plural] = n;

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
    const auto what = is_operation ? std::string("declared operations") :
        "the " + plural + " resource";
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
    b.set(doc, "is_operation", is_operation);
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
 * http_recipe_document: one section per route of @p routes, grouped under
 * the plural of @p n.
 */
void write_http_recipe(builder& b, value_id context, const recipe_names& n,
    const std::vector<value_id>& routes) {
    const auto& t = b.tables();
    const auto& component = n.component;
    const auto& singular = n.singular;
    const auto& plural = n.plural;

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

std::vector<std::string_view> message_field_types(const model_tables& t,
    value_id owner) {
    std::vector<std::string_view> out;
    for (const auto m : values::items(t, owner, "messages")) {
        for (const auto f : values::items(t, m, "fields"))
            out.push_back(values::str(t, f, "cpp_type"));
    }
    return out;
}

/*
 * utility_imports: ts_utility_imports of the messages of @p owner.
 */
void write_utility_imports(builder& b, value_id owner) {
    const auto cpp_types = message_field_types(b.tables(), owner);
    const auto utility = b.list(owner, "utility_imports");
    for (const auto& i : compass::model::ops::ts_utility_imports(cpp_types)) {
        const auto o = b.append_object(utility);
        b.set(o, "name_pascal", i.pascal);
        b.set(o, "module", i.module);
    }
}

/*
 * ts_protocol.ts.mustache's imports: utility_imports and the domain
 * interfaces other than the entity's own, extra_domain_imports.
 */
void write_ts_imports(builder& b, value_id entity,
    const types::entity_registry& registry) {
    write_utility_imports(b, entity);
    const auto& t = b.tables();
    const auto cpp_types = message_field_types(t, entity);
    const auto component = values::member(t, entity, "component");
    const auto owner = values::is(t, component,
        compass::model::types::value_kind::string) ?
        std::optional<std::string_view>(values::str(t, component)) :
        std::nullopt;
    // A copy: the builder's writes below may move the tables' strings.
    const std::string singular(values::str(t, entity, "entity_singular"));
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

constexpr std::string_view shell_recipe_template = "shell_recipe.org.mustache";
constexpr std::string_view http_recipe_template = "http_recipe.org.mustache";

/*
 * A domain entity's recipe names: its menu is its plural, or its singular
 * when it states none.
 */
recipe_names entity_recipe_names(const model_tables& t, value_id entity) {
    auto plural = text(t, entity, "entity_plural");
    auto singular = text(t, entity, "entity_singular");
    auto menu = !plural.empty() ? plural : singular;
    return {text(t, entity, "component"), std::move(menu), std::move(singular),
        std::move(plural)};
}

/*
 * The context of a junction template that reads more than the junction:
 * the shell and HTTP route units and recipes read its surface shape under
 * domain_entity, and the TypeScript protocol its utility imports.
 */
value_id junction_template_context(builder& b, value_id context,
    std::string_view model_key, std::string_view template_name,
    value_id surface) {
    const auto& t = b.tables();
    if (template_name == "ts_protocol.ts.mustache") {
        const auto copy = copy_of(b, context);
        write_utility_imports(b, values::member(t, copy, "junction"));
        write_utility_imports(b, values::member(t,
            values::member(t, copy, model_key), "junction"));
        return copy;
    }
    if (!template_name.starts_with("cpp_shell_command_") &&
        !template_name.starts_with("cpp_http_route_") &&
        template_name != shell_recipe_template &&
        template_name != http_recipe_template)
        return value_id::none();
    if (surface.is_none())
        return value_id::none();
    const auto copy = copy_of(b, context);
    const auto names = entity_recipe_names(t, surface);
    if (template_name == shell_recipe_template)
        write_shell_recipe(b, copy, names, values::items(b.tables(),
            values::member(b.tables(), surface, "shell"), "commands"),
            false);
    if (template_name == http_recipe_template)
        write_http_recipe(b, copy, names, values::items(b.tables(),
            values::member(b.tables(), surface, "http_route"), "routes"));
    b.set_copy(copy, "domain_entity", surface);
    return copy;
}

/*
 * _FAMILY_SERVICE_TEMPLATES: what reads a family's service view.
 */
bool is_family_service_template(std::string_view name) {
    constexpr std::string_view names[] = {"cpp_service.hpp.mustache",
        "cpp_service.cpp.mustache", "cpp_nats_handler.hpp.mustache",
        "cpp_nats_registrar.hpp.mustache", "cpp_nats_registrar.cpp.mustache"};
    return std::ranges::find(names, name) != std::end(names);
}

bool is_shell_aggregator_template(std::string_view name) {
    return name == "cpp_shell_command_aggregator_header.hpp.mustache" ||
        name == "cpp_shell_command_aggregator_impl.cpp.mustache";
}

/*
 * shell_aggregator: the command units of the models in the directory of
 * the model at @p site and the local units of its component's shell
 * surface, by name; none when the component has not opted in.
 */
value_id shell_aggregator_context(builder& b, value_id context,
    std::string_view model_type, const types::entity_registry& registry,
    const types::entity_site& site) {
    const auto& t = b.tables();
    const auto component = text(t, values::member(t, context, model_type),
        "component");
    const auto local = registry.shell_local_units.find(component);
    if (component.empty() || local == registry.shell_local_units.end())
        return value_id::none();
    std::map<std::string, bool, std::less<>> units;
    const auto dir = site.path.substr(0, site.path.rfind('/'));
    if (const auto it = registry.shell_units.find(dir);
        it != registry.shell_units.end()) {
        for (const auto& name : it->second)
            units[name + "_commands"] = false;
    }
    for (const auto& name : local->second)
        units[name] = true;
    const auto copy = copy_of(b, context);
    const auto o = b.object(copy, "shell_aggregator");
    b.set(o, "component", component);
    b.set(o, "component_upper", text::upper_snake(component));
    const auto list = b.list(o, "units");
    for (const auto& [name, is_local] : units) {
        const auto u = b.append_object(list);
        b.set(u, "unit", name);
        b.set(u, "local", is_local);
    }
    return copy;
}

/*
 * operation_http_route_plan: one POST route per shell command that states
 * :http_route:, its path the subject transliterated.
 */
void write_operation_http_route(builder& b, value_id operation) {
    const auto plan = b.object(operation, "http_route");
    const auto routes = b.list(plan, "routes");
    std::int64_t count = 0;
    for (const auto c : values::items(b.tables(), operation, "shell_commands")) {
        const auto& t = b.tables();
        if (!values::truthy(t, c, "http_route"))
            continue;
        const auto subject = text(t, c, "subject");
        const auto segments = text::split(subject, '.');
        if (segments.size() < 3)
            throw std::runtime_error(text(t, operation, "entity_singular") +
                ": message " + text(t, c, "request") + " states subject " +
                subject + ", which cannot be routed");
        auto pattern = "/api/" + std::string(segments[1]) + "/" +
            std::string(segments[0]) + "/" + std::string(segments[2]);
        std::string action;
        for (std::size_t i = 3; i < segments.size(); ++i)
            action += (i == 3 ? "" : "/") + std::string(segments[i]);
        if (!action.empty())
            pattern += "/" + action;
        const auto name = values::truthy(t, c, "operation") ?
            text(t, c, "operation") : text(t, c, "command");
        auto identifier = values::truthy(t, c, "operation_identifier") ?
            text(t, c, "operation_identifier") : name;
        std::ranges::replace(identifier, '-', '_');
        auto summary = name;
        std::ranges::replace(summary, '-', ' ');
        const bool requires_session = !values::truthy(t, c, "public");
        const auto rate_policy = values::member(t, c, "rate_policy");
        const bool has_fields = values::truthy(t, c, "positionals") ||
            values::truthy(t, c, "flags");
        const auto request = values::member(t, c, "request");
        const auto response = values::member(t, c, "response_type");

        const auto o = b.append_object(routes);
        b.set(o, "command", name);
        b.set(o, "identifier", identifier);
        b.set(o, "method", "post");
        b.set(o, "method_upper", "POST");
        b.set(o, "pattern", pattern);
        b.set(o, "summary", text::capitalize(summary));
        b.set(o, "description", "Forwards to the " + subject + " operation.");
        b.set(o, "subject", subject);
        b.set_copy(o, "request", request);
        b.set_copy(o, "response_type", response);
        b.set(o, "requires_session", requires_session);
        if (rate_policy.is_none())
            b.set(o, "rate_policy", "");
        else
            b.set_copy(o, "rate_policy", rate_policy);
        b.set(o, "has_fields", has_fields);
        ++count;
    }
    b.set_integer(plan, "route_count", count);
}

/*
 * The context of an operation template that reads more than the
 * operation: the HTTP route units and recipe read its route plan, and the
 * recipes their documents, filed under its entity singular.
 */
value_id operation_template_context(builder& b, value_id context,
    std::string_view model_key, std::string_view template_name) {
    const bool routes =
        template_name.starts_with("cpp_http_route_operation_") ||
        template_name == http_recipe_template;
    if (!routes && template_name != shell_recipe_template)
        return value_id::none();
    const auto copy = copy_of(b, context);
    const auto top = values::member(b.tables(), copy, "operation");
    if (top.is_none())
        return copy;
    if (routes) {
        write_operation_http_route(b, top);
        write_operation_http_route(b, values::member(b.tables(),
            values::member(b.tables(), copy, model_key), "operation"));
    }
    const auto& t = b.tables();
    const auto singular = text(t, top, "entity_singular");
    const recipe_names names{text(t, top, "component"), singular, singular,
        singular};
    if (template_name == shell_recipe_template)
        write_shell_recipe(b, copy, names,
            values::items(t, top, "shell_commands"), true);
    if (template_name == http_recipe_template)
        write_http_recipe(b, copy, names, values::items(b.tables(),
            values::member(b.tables(), top, "http_route"), "routes"));
    return copy;
}

}

std::string recipe_org_id(std::string_view name) {
    constexpr std::array<std::uint8_t, 16> space{0x6F, 0x1D, 0x2C, 0x3A, 0x7E,
        0x4B, 0x4C, 0x58, 0x9A, 0x21, 0x8D, 0x3F, 0x5B, 0x7C, 0x0E, 0x64};
    return uuid5(space, name);
}

std::string uuid5(const std::array<std::uint8_t, 16>& space,
    std::string_view name) {
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
    std::string_view template_name, const types::entity_registry& registry,
    const types::entity_site& site, value_id prepared) {
    const auto& t = b.tables();
    if ((model_type == "domain_entity" || model_type == "junction") &&
        is_shell_aggregator_template(template_name))
        return shell_aggregator_context(b, context, model_type, registry,
            site);
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
    if (model_type == "junction")
        return junction_template_context(b, context, model_key, template_name,
            prepared);
    if (model_type == "family")
        return is_family_service_template(template_name) ? prepared :
            value_id::none();
    if (model_type == "operation")
        return operation_template_context(b, context, model_key,
            template_name);
    if (model_type != "domain_entity")
        return value_id::none();
    const auto entity = values::member(t, context, "domain_entity");
    if (template_name == shell_recipe_template) {
        const auto copy = copy_of(b, context);
        write_shell_recipe(b, copy, entity_recipe_names(t, entity),
            values::items(t, values::member(t, entity, "shell"), "commands"),
            false);
        return copy;
    }
    if (template_name == http_recipe_template) {
        const auto copy = copy_of(b, context);
        write_http_recipe(b, copy, entity_recipe_names(t, entity),
            values::items(t, values::member(t, entity, "http_route"),
                "routes"));
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
