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
#include "compass.codegen/ops/entity_steps.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>
#include "compass.codegen/ops/entity_registry.hpp"
#include "compass.codegen/ops/values.hpp"
#include "entity_text.hpp"

namespace compass::codegen::ops {

using compass::model::ops::builder;
using compass::model::types::value_id;

namespace {

bool is_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_';
}

bool is_lower_name(std::string_view s) {
    return !s.empty() && s.front() >= 'a' && s.front() <= 'z' &&
        std::ranges::all_of(s, [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        });
}

/*
 * The header list _member_headers builds, in first-seen order.
 */
class member_headers {
public:
    member_headers(const types::entity_registry& registry,
        const types::model_directory& directory, std::string root,
        std::string own, std::string_view model_path)
        : registry_(registry), directory_(directory), root_(std::move(root)),
          own_(std::move(own)), model_path_(model_path) {}

    void add(std::string header) {
        if (std::ranges::find(out_, header) == out_.end())
            out_.push_back(std::move(header));
    }

    void visit(std::string_view cpp_type) {
        qualified(cpp_type);
        constexpr std::string_view marker = "domain::";
        for (auto at = cpp_type.find(marker); at != std::string_view::npos;
             at = cpp_type.find(marker, at + 1)) {
            if (at != 0 && (is_word(cpp_type[at - 1]) || cpp_type[at - 1] == ':'))
                continue;
            add_named(word_at(cpp_type, at + marker.size()), root_);
        }
        std::size_t i = 0;
        while (i < cpp_type.size()) {
            if (!is_word(cpp_type[i]) && cpp_type[i] != ':') {
                ++i;
                continue;
            }
            auto end = i;
            while (end < cpp_type.size() &&
                   (is_word(cpp_type[end]) || cpp_type[end] == ':'))
                ++end;
            const auto run = cpp_type.substr(i, end - i);
            if (is_lower_name(run))
                add_named(run, {});
            i = end;
        }
    }

    std::vector<std::string> take() { return std::move(out_); }

private:
    static std::string_view word_at(std::string_view s, std::size_t at) {
        auto end = at;
        while (end < s.size() && is_word(s[end]))
            ++end;
        return s.substr(at, end - at);
    }

    /*
     * ((?:\w+::)+)domain::(\w+): the namespace run before each domain::
     * that has one.
     */
    void qualified(std::string_view cpp_type) {
        constexpr std::string_view marker = "domain::";
        std::size_t from = 0;
        for (auto at = cpp_type.find(marker, from); at != std::string_view::npos;
             at = cpp_type.find(marker, from)) {
            auto start = at;
            while (start >= 3 && cpp_type[start - 1] == ':' &&
                   cpp_type[start - 2] == ':' && is_word(cpp_type[start - 3])) {
                auto word = start - 3;
                while (word > 0 && is_word(cpp_type[word - 1]))
                    --word;
                start = word;
            }
            const auto name = word_at(cpp_type, at + marker.size());
            from = at + marker.size();
            if (start == at || name.empty())
                continue;
            from = at + marker.size() + name.size();
            std::vector<std::string_view> parts;
            for (const auto p : text::split(cpp_type.substr(start, at - start),
                     ':')) {
                if (!p.empty())
                    parts.push_back(p);
            }
            if (parts.size() != 2)
                throw std::runtime_error(std::string(model_path_) +
                    ": member type '" + std::string(cpp_type) +
                    "' names a namespace this generator does not know");
            if (parts[1] == "utility")
                continue;
            const auto owner = std::string(parts[0]) + "." + std::string(parts[1]);
            if (owner == own_)
                add_named(name, root_);
            else
                foreign(owner, name);
        }
    }

    void add_named(std::string_view name, std::string_view fallback) {
        if (directory_.group_names.contains(name))
            add(root_ + "/domain/" + std::string(name) + ".hpp");
        else if (directory_.member_names.contains(name))
            add(root_ + "/domain/" + std::string(name) + "_codec.hpp");
        else if (!fallback.empty())
            add(std::string(fallback) + "/domain/" + std::string(name) + ".hpp");
    }

    /*
     * _foreign_root: the include root of another component's domain type,
     * flat or under its api part, as the header that exists decides.
     */
    void foreign(const std::string& owner, std::string_view name) {
        const std::string candidates[][2] = {
            {owner, "projects/" + owner + "/include/" + owner},
            {owner + ".api", "projects/" + owner + "/api/include/" + owner +
                ".api"}};
        const std::string* found = nullptr;
        std::size_t count = 0;
        for (const auto& [root, base] : candidates) {
            const auto header = base + "/domain/" + std::string(name);
            if (registry_.domain_headers.contains(header + ".hpp")) {
                ++count;
                found = &root;
            }
        }
        if (count != 1)
            throw std::runtime_error(std::string(model_path_) + ": " + owner +
                "'s " + std::string(name) + " needs exactly one domain header");
        const auto base = *found == owner ?
            candidates[0][1] : candidates[1][1];
        const auto codec = registry_.domain_headers.contains(
            base + "/domain/" + std::string(name) + "_codec.hpp");
        add(*found + "/domain/" + std::string(name) + (codec ? "_codec" : "") +
            ".hpp");
    }

    const types::entity_registry& registry_;
    const types::model_directory& directory_;
    std::string root_;
    std::string own_;
    std::string_view model_path_;
    std::vector<std::string> out_;
};

member_headers headers_for(const builder& b, value_id entity,
    const types::entity_registry& registry, const types::entity_site& site) {
    const auto& t = b.tables();
    const auto include = values::truthy(t, entity, "component_include") ?
        values::py_str(t, values::member(t, entity, "component_include")) :
        values::py_str(t, values::member(t, entity, "component"), "None") +
            "." + values::py_str(t, values::member(t, entity, "subcomponent"),
                "None");
    return member_headers(registry, directory_of(registry, site.path),
        "ores." + include,
        "ores." + values::py_str(t, values::member(t, entity, "component")),
        site.path);
}

void write_list(builder& b, value_id entity, std::string_view key,
    const std::vector<std::string>& items) {
    const auto list = b.list(entity, key);
    for (const auto& i : items)
        b.append(list, i);
}

}

void derive_codec_includes(builder& b, value_id entity,
    const types::entity_registry& registry, const types::entity_site& site) {
    const auto& t = b.tables();
    auto row = headers_for(b, entity, registry, site);
    for (const auto g : values::items(t, entity, "domain_groups")) {
        if (!values::truthy(t, g, "header"))
            continue;
        auto header = std::string(values::str(t, g, "header"));
        const auto first = header.find_first_not_of('"');
        header = first == std::string::npos ? std::string() :
            header.substr(first, header.find_last_not_of('"') - first + 1);
        row.add(header);
    }
    if (values::truthy(t, entity, "has_audit_group")) {
        std::vector<std::string_view> parts;
        for (const auto p : text::split(
                 values::str(t, entity, "audit_group_qualified"), ':')) {
            if (!p.empty() && p != "domain")
                parts.push_back(p);
        }
        if (parts.size() == 3)
            row.add(std::string(parts[0]) + "." + std::string(parts[1]) +
                ".api/domain/" + std::string(parts[2]) + ".hpp");
    }
    std::vector<value_id> columns = values::items(t, entity, "columns");
    std::ranges::copy(values::items(t,
        values::member(t, entity, "primary_key"), "columns"),
        std::back_inserter(columns));
    std::ranges::copy(values::items(t, entity, "natural_keys"),
        std::back_inserter(columns));
    for (const auto c : columns)
        row.visit(values::py_str(t, values::member(t, c, "cpp_type")));

    auto messages = headers_for(b, entity, registry, site);
    for (const auto m : values::items(t, entity, "messages")) {
        for (const auto f : values::items(t, m, "fields"))
            messages.visit(values::py_str(t, values::member(t, f, "cpp_type")));
    }
    write_list(b, entity, "wire_codec_includes", row.take());
    write_list(b, entity, "message_codec_includes", messages.take());
    b.set(entity, "has_generated_wire_codec", site.has_generated_wire_codec);
}

}
