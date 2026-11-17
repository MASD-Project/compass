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
#include "compass.cpp/codecs/registrars.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.codec/types/fact.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::cpp::codecs {

namespace {

namespace ct = compass::codec::types;
namespace pt = compass::platform::text;

/**
 * @brief A generated registrar: the entity its declaration names, its
 * header and the offset of the declaration there.
 */
struct registrar {
    std::string entity;
    std::string_view header;
    std::uint32_t offset;
};

std::set<std::string> names_in(const pt::regex& declaration,
    std::string_view text) {
    std::set<std::string> out;
    for (const auto& m : declaration.find_all(text))
        out.emplace(m.group(1));
    return out;
}

/**
 * @brief The registrars the headers under @p include whose names end in
 * @p suffix declare, the last header of an entity winning, as the
 * script's dictionary keeps it.
 */
std::map<std::string, registrar> registrars_under(const ct::corpus_view& v,
    const std::string& include, std::string_view suffix,
    const pt::regex& declaration, std::string_view excluded = {}) {
    auto headers = v.list_files(include, suffix);
    std::ranges::sort(headers, ops::component_less);
    std::map<std::string, registrar> out;
    for (const auto h : headers) {
        if (!excluded.empty() && h.ends_with(excluded))
            continue;
        const auto text = v.read_file(h).value_or("");
        if (const auto m = declaration.search(text)) {
            const std::string entity(m->group(1));
            out[entity] = {entity, h, static_cast<std::uint32_t>(m->start())};
        }
    }
    return out;
}

/**
 * @brief Reports each registrar of @p wanted that @p composition does not
 * call, or the missing composition file once.
 */
void check_composed(const ct::corpus_view& v, ct::diagnostic_table& out,
    std::string_view component, const std::map<std::string, registrar>& wanted,
    const std::string& composition, const pt::regex& declaration,
    std::string_view kind) {
    if (wanted.empty())
        return;
    const auto text = v.read_file(composition);
    if (!text) {
        const auto& first = wanted.begin()->second;
        ops::report(out, first.header, first.offset, 1, std::string(component) +
            " generates " + std::to_string(wanted.size()) + " " +
            std::string(kind) + " registrar(s) but has no " + composition,
            "add " + composition + " and call each registrar from it");
        return;
    }
    const auto composed = names_in(declaration, *text);
    for (const auto& [entity, r] : wanted) {
        if (composed.contains(entity))
            continue;
        ops::report(out, r.header, r.offset, 0, std::string(component) +
            ": the " + std::string(kind) + " registrar of " + entity +
            " is not composed", "call it from " + composition);
    }
}

constexpr std::array<ct::rule, 2> composed_rules{{
    {"uncomposed_registrar", "Every generated registrar of a checked "
        "component is called from its composition file."},
    {"missing_composition", "A checked component that generates registrars "
        "has its composition file."}}};

constexpr std::array<std::string_view, 3> event_components{"ores.dq",
    "ores.refdata", "ores.synthetic"};

void run_events(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex declaration(R"(register_(\w+)_event_mapping\s*\()");
    for (const auto c : event_components) {
        const auto root = "projects/" + std::string(c) + "/service/";
        check_composed(v, out, c, registrars_under(v, root + "include/",
            "_event_registrar.hpp", declaration), root +
            "src/messaging/event_registrar.cpp", declaration, "event");
    }
}

constexpr std::array<std::string_view, 2> history_components{
    "ores.refdata", "ores.reporting"};

void run_history(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex declaration(
        R"(register_(\w+)_history_provider\s*\()");
    for (const auto c : history_components) {
        const auto root = "projects/" + std::string(c) + "/core/";
        check_composed(v, out, c, registrars_under(v, root + "include/",
            "_history_provider_registrar.hpp", declaration), root +
            "src/messaging/registrar.cpp", declaration, "history-provider");
    }
}

constexpr std::array<ct::rule, 1> handler_rules{{
    {"registrar_without_responder", "Every generated handler registrar is "
        "called, or every subject it subscribes is subscribed by hand."}}};

constexpr std::array<std::string_view, 8> handler_components{"ores.assets",
    "ores.compute", "ores.dq", "ores.marketdata", "ores.scheduler",
    "ores.synthetic", "ores.trading", "ores.variability"};

const pt::regex& handler_declaration() {
    static const pt::regex r(R"(register_(\w+)_handlers\s*\()");
    return r;
}

const pt::regex& handler_subject() {
    static const pt::regex r(R"((\w+_request)::nats_subject)");
    return r;
}

std::vector<std::string_view> handler_files(const ct::corpus_view& v) {
    std::vector<std::string_view> out;
    for (const auto c : handler_components) {
        const auto root = "projects/" + std::string(c) + "/";
        for (const auto suffix : {".cpp", ".hpp"}) {
            const auto paths = v.list_files(root, suffix);
            out.insert(out.end(), paths.begin(), paths.end());
        }
    }
    return out;
}

/**
 * @brief The fact of a file: the entity and offset of the handler
 * registrar it declares, or an empty entity, then the registrars it calls
 * and the request subjects it names.
 */
void map_handlers(std::string_view, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    ct::fact_writer fact(out.fact);
    if (const auto m = handler_declaration().search(bytes))
        fact.text(m->group(1))
            .number(static_cast<std::uint32_t>(m->start()));
    else
        fact.text({}).number(0);
    const auto text = pt::decode_ignoring(bytes);
    const auto called = names_in(handler_declaration(), text);
    fact.number(static_cast<std::uint32_t>(called.size()));
    for (const auto& n : called)
        fact.text(n);
    for (const auto& n : names_in(handler_subject(), text))
        fact.text(n);
}

struct handler_fact {
    std::string_view entity;
    std::uint32_t offset = 0;
    std::vector<std::string> called;
    std::set<std::string> subjects;
};

handler_fact read_handler_fact(std::string_view f) {
    ct::fact_reader r(f);
    handler_fact out;
    out.entity = r.text();
    out.offset = r.number();
    for (auto n = r.number(); n > 0; --n)
        out.called.emplace_back(r.text());
    while (!r.done())
        out.subjects.emplace(r.text());
    return out;
}

void reduce_handlers(std::span<const ct::scanned_file> files,
    const ct::corpus_view& v, ct::diagnostic_table& out) {
    std::unordered_map<std::string_view, std::string_view> facts;
    for (const auto& f : files)
        facts.emplace(f.path, f.fact);
    const auto fact_of = [&](std::string_view path) {
        const auto it = facts.find(path);
        return read_handler_fact(it == facts.end() ? std::string_view{}
            : it->second);
    };
    for (const auto c : handler_components) {
        const auto root = "projects/" + std::string(c) + "/";
        auto headers = v.list_files(root + "core/include/",
            "_registrar.hpp");
        std::ranges::sort(headers, ops::component_less);
        std::map<std::string, registrar> entities;
        for (const auto h : headers) {
            if (h.ends_with("_history_provider_registrar.hpp"))
                continue;
            const auto f = fact_of(h);
            if (!f.entity.empty()) {
                const std::string entity(f.entity);
                entities[entity] = {entity, h, f.offset};
            }
        }
        if (entities.empty())
            continue;
        std::set<std::string> called;
        std::set<std::string> named;
        std::map<std::string, std::set<std::string>> subscribes;
        for (const auto suffix : {".cpp", ".hpp"}) {
            for (const auto p : v.list_files(root, suffix)) {
                const auto parts = ops::components_of(p);
                if (std::ranges::find(parts, "tests") != parts.end())
                    continue;
                auto f = fact_of(p);
                const auto stem = ops::stem_of(p);
                std::string own;
                if (stem.ends_with("_registrar"))
                    own = stem.substr(0, stem.size() - 10);
                const bool is_own = stem.ends_with("_registrar");
                if (is_own && ops::suffix_of(p) == ".cpp")
                    subscribes[own] = f.subjects;
                for (const auto& n : f.called) {
                    if (!is_own || n != own)
                        called.insert(n);
                }
                if (!is_own)
                    named.merge(f.subjects);
            }
        }
        for (const auto& [entity, r] : entities) {
            if (called.contains(entity))
                continue;
            const auto s = subscribes.find(entity);
            if (s != subscribes.end() && !s->second.empty() &&
                std::ranges::includes(named, s->second))
                continue;
            ops::report(out, r.header, r.offset, 0, std::string(c) +
                ": the handler registrar of " + entity + " is not called, so "
                "its subjects have no responder",
                "call it from the component's core/src/messaging/"
                "registrar.cpp");
        }
    }
}

}

const ct::descriptor event_registrars{
    .name = "event_registrars",
    .version = 1,
    .summary = "Reports each generated event registrar its component's "
        "event_registrar.cpp does not call.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = composed_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run_events}};

const ct::descriptor history_registrars{
    .name = "history_registrars",
    .version = 1,
    .summary = "Reports each generated history-provider registrar its "
        "component's registrar.cpp does not call.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = composed_rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run_history}};

const ct::descriptor handler_registrars{
    .name = "handler_registrars",
    .version = 2,
    .summary = "Reports each generated handler registrar that nothing calls "
        "and whose subjects nothing else subscribes.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = handler_rules,
    .enabled_by_default = false,
    .run = ct::scan_function{handler_files, map_handlers,
        reduce_handlers}};

}
