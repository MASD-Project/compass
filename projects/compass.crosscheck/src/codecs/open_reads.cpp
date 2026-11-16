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
#include "compass.crosscheck/codecs/open_reads.hpp"

#include <array>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::uint32_t open_read = 0;
constexpr std::uint32_t unguarded_history = 1;
constexpr std::uint32_t stale_allow = 2;
constexpr std::uint32_t no_allow_list = 3;

constexpr std::array<ct::rule, 4> rules{{
    {"open_read", "A generated handler's read checks a permission, or "
        "Authorised reads allow-lists its subject."},
    {"unguarded_history", "Every history provider a generated registrar "
        "registers names a literal read code."},
    {"stale_allow", "Every subject Authorised reads allow-lists is defined "
        "by a protocol header."},
    {"no_allow_list", "Authorised reads has its allow-list section."}}};

constexpr std::array<ct::option, 1> options{{
    {"allow_list", "doc/knowledge/security/authorised-reads.org",
        "The page whose \"** The allow-list\" table names the subjects "
        "that may be read with no permission check."}}};

constexpr std::string_view protocols =
    "projects/ores.*/api/include/*/messaging/*_protocol.hpp";

std::string read(const ct::corpus_view& v, std::string_view path) {
    return pt::universal_newlines(pt::decode_replacing(
        v.read_file(path).value_or("")));
}

const pt::regex& nats_subject() {
    static const pt::regex out(R"re(nats_subject\s*=\s*"([a-z0-9_.-]+)")re");
    return out;
}

std::optional<std::set<std::string>> allow_list(std::string_view doc) {
    static const pt::regex heading(R"(^\*\* The allow-list\s*$)",
        {.multiline = true});
    static const pt::regex following(R"(^\*\* )", {.multiline = true});
    static const pt::regex row(R"(^\| =([a-z0-9_.-]+)= \|)",
        {.multiline = true});
    const auto h = heading.search(doc);
    if (!h)
        return std::nullopt;
    const auto rest = doc.substr(h->end());
    const auto next = following.search(rest);
    const auto end = next ? h->end() + next->start() : doc.size();
    std::set<std::string> out;
    for (const auto& m : row.find_all(doc.substr(h->start(),
             end - h->start())))
        out.emplace(m.group(1));
    return out;
}

/**
 * @brief Each protocol request struct and the subject it addresses, the
 * later header winning.
 */
std::map<std::string, std::string> subjects_by_request(
    const ct::corpus_view& v, std::set<std::string>& all) {
    static const pt::regex request(R"(struct (\w+_request) \{)");
    std::map<std::string, std::string> out;
    for (const auto path : co::glob(v, protocols)) {
        const auto text = read(v, path);
        for (const auto& m : nats_subject().find_all(text))
            all.emplace(m.group(1));
        std::optional<std::string> current;
        for (const auto line : co::split_lines(text)) {
            if (const auto s = request.match(line)) {
                current = std::string(s->group(1));
                continue;
            }
            const auto subject = nats_subject().search(line);
            if (subject && current) {
                out[*current] = std::string(subject->group(1));
                current.reset();
            }
        }
    }
    return out;
}

/**
 * @brief (method, subject) for each read method of @p handler that checks
 * no permission, as the script's open_reads finds them.
 */
std::vector<std::pair<std::string, std::string>> unchecked_reads(
    std::string_view handler,
    const std::map<std::string, std::string>& requests) {
    static const pt::regex method(
        R"(^    void (\w+)\(ores::nats::message msg\) \{$)",
        {.multiline = true});
    static const pt::regex decode(R"(decode<(\w+_request)>)");
    static const pt::regex serves(R"(@brief Serves ([a-z0-9_.-]+)\.)");
    std::vector<std::pair<std::string, std::string>> out;
    const auto starts = method.find_all(handler);
    for (std::size_t i = 0; i < starts.size(); ++i) {
        const std::string name(starts[i].group(1));
        const auto end = i + 1 < starts.size() ? starts[i + 1].start()
                                               : handler.size();
        const auto body = handler.substr(starts[i].end(),
            end - starts[i].end());
        std::set<std::string> decoded;
        for (const auto& d : decode.find_all(body)) {
            if (const auto it = requests.find(std::string(d.group(1)));
                it != requests.end())
                decoded.insert(it->second);
        }
        if (!name.starts_with("list_") && !name.starts_with("get_") &&
            decoded.empty())
            continue;
        if (body.contains("has_permission("))
            continue;
        auto before = handler.substr(0, starts[i].start());
        while (!before.empty() && co::is_space(before.back()))
            before.remove_suffix(1);
        std::string_view comment;
        if (before.ends_with("*/")) {
            const auto open = before.rfind("/**");
            comment = before.substr(open == std::string_view::npos
                ? before.size() - 1 : open);
        }
        if (const auto s = serves.search(comment))
            out.emplace_back(name, std::string(s->group(1)));
        else if (!decoded.empty())
            for (const auto& subject : decoded)
                out.emplace_back(name, subject);
        else
            out.emplace_back(name, "an unnamed subject");
    }
    return out;
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex history_call(R"(register_history_provider\s*\()");
    static const pt::regex history_code(
        R"re(register_history_provider\(\s*"[^"]+",\s*"([^"]*)",)re");
    const auto doc_path = v.option("allow_list");
    const auto allowed = allow_list(read(v, doc_path));
    if (!allowed) {
        co::report(out, doc_path, 0, no_allow_list, "the page has no '** The "
            "allow-list' section to read", "Restore the allow-list section "
            "of Authorised reads.");
        return;
    }
    std::set<std::string> subjects;
    const auto requests = subjects_by_request(v, subjects);

    for (const auto path : co::glob(v,
             "projects/ores.*/core/include/*/messaging/*_handler.hpp")) {
        const auto text = read(v, path);
        if (!text.contains("Template: cpp_nats_handler.hpp.mustache"))
            continue;
        for (const auto& [method, subject] :
            unchecked_reads(text, requests)) {
            if (allowed->contains(subject))
                continue;
            co::report(out, path, 0, open_read, method + " serves " +
                subject + " with no permission check, and Authorised reads "
                "does not allow-list it", "Check a permission in the "
                "handler's model, or allow-list the subject in Authorised "
                "reads with its reason.");
        }
    }
    for (const auto path : co::glob(v, "projects/ores.*/core/src/messaging/"
             "*_history_provider_registrar.cpp")) {
        const auto text = read(v, path);
        if (!text.contains(
                "Template: cpp_history_provider_registrar.cpp.mustache"))
            continue;
        const auto calls = history_call.find_all(text).size();
        const auto codes = history_code.find_all(text);
        if (codes.size() != calls)
            co::report(out, path, 0, unguarded_history, "registers " +
                std::to_string(calls) + " history provider(s) but " +
                std::to_string(codes.size()) + " name a literal read code",
                "Give each history its read code in the model.");
        for (const auto& c : codes) {
            if (c.group(1).empty())
                co::report(out, path, 0, unguarded_history, "registers a "
                    "history with no read code", "Give each history its "
                    "read code in the model.");
        }
    }
    for (const auto& subject : *allowed) {
        if (subjects.contains(subject))
            continue;
        co::report(out, doc_path, 0, stale_allow, "allow-lists " + subject +
            ", which no protocol header defines", "Remove the row, or "
            "restore the subject.");
    }
}

}

const ct::descriptor open_reads{
    .name = "open_reads",
    .version = 1,
    .summary = "Reports each generated read with no permission check that "
        "Authorised reads does not allow-list.",
    .reads = ct::inputs::of_types({}),
    .options = options,
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
