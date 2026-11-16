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
#include "compass.crosscheck/codecs/model_drift.hpp"

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/glob.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/words.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

enum rule : std::uint32_t { unknown_profile, contradicted, profile_conflict,
    unset_required, self_contradicting, no_profiles };

constexpr std::array<ct::rule, 6> rules{{
    {"unknown_profile", "A model binds only to profiles that exist."},
    {"contradicted", "A model does not redeclare a feature its profile "
        "fixes to another value."},
    {"profile_conflict", "The profiles a model binds to agree on every "
        "feature they fix."},
    {"unset_required", "A model sets every feature its profile requires."},
    {"self_contradicting", "A model states a feature once, or always with "
        "one value."},
    {"no_profiles", "The tree has profiles to bind to."}}};

std::string read(const ct::corpus_view& v, std::string_view path) {
    return pt::universal_newlines(pt::decode_replacing(
        v.read_file(path).value_or("")));
}

/**
 * @brief Each profile's slug and the feature values it fixes, in order.
 */
std::map<std::string, std::vector<std::pair<std::string, std::string>>>
profiles(const ct::corpus_view& v) {
    static const pt::regex assignment(
        R"(^\|\s*(?:\[\[id:[^\]]*\]\[)?([a-z_]+)\]?\]?\s*\|\s*([^|]+?)\s*\|\s*$)");
    std::map<std::string, std::vector<std::pair<std::string, std::string>>>
        out;
    for (const auto path : co::glob(v, "projects/modeling/variability_*.org")) {
        const auto text = read(v, path);
        if (!text.contains("#+type: profile"))
            continue;
        std::vector<std::pair<std::string, std::string>> fixed;
        bool in_section = false;
        for (const auto line : co::split_lines(text)) {
            if (line.starts_with("* ")) {
                in_section = co::strip(line) == "* Assignments";
                continue;
            }
            if (!in_section)
                continue;
            const auto m = assignment.match(line);
            if (!m)
                continue;
            const std::string feature(m->group(1));
            const std::string value(co::strip(m->group(2)));
            if (feature == "Feature" || feature == "feature" ||
                value.find_first_not_of("-+") == std::string::npos)
                continue;
            const auto it = std::ranges::find_if(fixed, [&](const auto& f) {
                return f.first == feature; });
            if (it != fixed.end())
                it->second = value;
            else
                fixed.emplace_back(feature, value);
        }
        auto slug = std::string(co::stem_of(path).substr(
            std::string_view("variability_").size()));
        std::ranges::replace(slug, '_', '-');
        out[slug] = std::move(fixed);
    }
    return out;
}

/**
 * @brief Every value a model states for each feature, with the heading
 * path it sits under, in the order of the drawers.
 */
std::map<std::string, std::vector<std::pair<std::string, std::string>>>
model_flags(std::string_view text) {
    static const pt::regex property(R"(^:([a-z_]+):\s*(.+?)\s*$)");
    std::map<std::string, std::vector<std::pair<std::string, std::string>>>
        out;
    std::vector<std::string> stack;
    bool in_drawer = false;
    for (const auto line : co::split_lines(text)) {
        const auto stripped = co::strip(line);
        if (stripped.starts_with('*') && stripped.contains(' ')) {
            const auto stars = stripped.find_first_not_of('*');
            stack.resize(std::min(stack.size(), stars - 1));
            stack.emplace_back(co::strip(stripped.substr(stars)));
            in_drawer = false;
            continue;
        }
        if (stripped == ":PROPERTIES:") {
            in_drawer = true;
            continue;
        }
        if (stripped == ":END:") {
            in_drawer = false;
            continue;
        }
        if (!in_drawer)
            continue;
        const auto m = property.match(stripped);
        if (!m)
            continue;
        std::string where;
        for (const auto& s : stack)
            where += (where.empty() ? "" : " / ") + s;
        out[std::string(m->group(1))].emplace_back(
            where.empty() ? "(top)" : where, std::string(co::strip(
                m->group(2))));
    }
    return out;
}

/**
 * @brief What a model states for a feature: its one value, none, or the
 * entries when they disagree.
 */
struct statement {
    std::optional<std::string> value;
    const std::vector<std::pair<std::string, std::string>>* disagreement =
        nullptr;
};

statement stated(const std::map<std::string, std::vector<std::pair<
    std::string, std::string>>>& values, const std::string& feature) {
    const auto it = values.find(feature);
    if (it == values.end() || it->second.empty())
        return {};
    std::set<std::string> distinct;
    for (const auto& [where, value] : it->second)
        distinct.insert(value);
    if (distinct.size() > 1)
        return {std::nullopt, &it->second};
    return {it->second.front().second, nullptr};
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    const auto all = profiles(v);
    if (all.empty()) {
        co::report(out, "projects/modeling", 0, no_profiles, "no profiles "
            "found under projects/modeling", "The check reads "
            "projects/modeling/variability_*.org.");
        return;
    }
    const auto fix = "A model that names a profile must not redeclare a "
        "feature the profile fixes to another value.";
    for (const auto path : co::glob(v, "projects/*/modeling/*.org")) {
        const auto values = model_flags(read(v, path));
        const auto bound = stated(values, "profile");
        if (!bound.value || bound.value->empty())
            continue;
        std::map<std::string, std::pair<std::string, std::string>> fixed_by;
        for (const auto raw : ops::split(*bound.value, ',')) {
            const std::string name(co::strip(raw));
            if (name.empty())
                continue;
            const auto profile = all.find(name);
            if (profile == all.end()) {
                co::report(out, path, 0, unknown_profile, "binds to unknown "
                    "profile '" + name + "'", fix);
                continue;
            }
            for (const auto& [feature, expected] : profile->second) {
                if (const auto it = fixed_by.find(feature);
                    it != fixed_by.end() && it->second.second != expected) {
                    co::report(out, path, 0, profile_conflict, "profiles '" +
                        it->second.first + "' and '" + name + "' disagree on " +
                        feature + " (" + it->second.second + " vs " +
                        expected + ")", fix);
                    continue;
                }
                fixed_by[feature] = {name, expected};
                const auto actual = stated(values, feature);
                if (actual.disagreement) {
                    std::string where;
                    for (const auto& [w, value] : *actual.disagreement)
                        where += (where.empty() ? "" : "; ") + w + "=" + value;
                    co::report(out, path, 0, self_contradicting, "states " +
                        feature + " twice with different values (" + where +
                        ")", fix);
                    continue;
                }
                if (expected.starts_with("required")) {
                    if (!actual.value || actual.value->empty())
                        co::report(out, path, 0, unset_required, "profile '" +
                            name + "' requires " + feature + " to be set, "
                            "model states none", fix);
                    continue;
                }
                if (actual.value && *actual.value != expected)
                    co::report(out, path, 0, contradicted, "profile '" + name +
                        "' fixes " + feature + "=" + expected + ", model "
                        "declares " + *actual.value, fix);
            }
        }
    }
}

}

const ct::descriptor model_drift{
    .name = "model_drift",
    .version = 1,
    .summary = "Reports each model that contradicts or leaves unset a "
        "feature its profile fixes.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
