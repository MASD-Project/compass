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
#include "compass.env/ops/generate.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <set>
#include "compass.env/ops/format.hpp"
#include "compass.env/ops/templates.hpp"
#include "compass.ingest/org/parser.hpp"
#include "compass.model/ops/outline.hpp"
#include "compass.org/types/tables.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::env::ops {

namespace {

using types::value_kind;
using failure = std::unexpected<std::string>;

constexpr std::size_t default_secret_length = 32;

std::vector<std::string_view> lines_of(std::string_view text) {
    std::vector<std::string_view> out;
    std::size_t at = 0;
    while (at < text.size()) {
        auto end = text.find('\n', at);
        if (end == std::string_view::npos)
            end = text.size();
        auto line = text.substr(at, end - at);
        if (line.ends_with('\r'))
            line.remove_suffix(1);
        out.push_back(line);
        at = end + 1;
    }
    return out;
}

std::string_view nonempty(const values& m, std::string_view key) {
    const auto it = m.find(key);
    return it == m.end() ? std::string_view{} : std::string_view(it->second);
}

/**
 * @brief The service fields the templates of @p v use.
 */
std::vector<std::string> fields_used(const types::variable& v) {
    std::vector<std::string> out;
    for (const auto text : {std::string_view(v.name),
             std::string_view(v.argument), std::string_view(v.when)}) {
        const auto names = names_in(text);
        if (!names)
            continue;
        for (const auto& n : *names)
            if (n.starts_with("service."))
                out.push_back(n.substr(8));
    }
    return out;
}

/**
 * @brief The state of one generation.
 */
class writer {
public:
    writer(const types::env_format& f, const sources& s) : f_(f), s_(s) {}

    std::expected<void, std::string> row(const types::variable& v,
        const types::service* service) {
        const resolver resolve = [this, service](std::string_view name) {
            return lookup(name, service);
        };
        if (v.kind == value_kind::carried)
            return carried(v, resolve);
        const auto name = expand(v.name, resolve);
        if (!name)
            return failure(std::format("{}: {}", v.name, name.error()));
        auto value = value_of(v, *name, resolve);
        if (!value)
            return failure(std::format("{}: {}", *name, value.error()));
        if (const auto it = s_.overrides.find(*name);
            it != s_.overrides.end()) {
            *value = it->second;
            used_.insert(*name);
        }
        computed_[*name] = *value;
        if (!v.when.empty()) {
            const auto when = expand(v.when, resolve);
            if (!when)
                return failure(std::format("{}: {}", *name, when.error()));
            if (when->empty())
                return {};
        }
        if (const auto r = blocks(v, resolve); !r)
            return r;
        text_ += *name + "=" + *value + "\n";
        names_.push_back(*name);
        return {};
    }

    std::expected<generated, std::string> finish() {
        for (const auto& [name, value] : s_.overrides)
            if (!used_.contains(name))
                return failure(std::format("--var {}: no variable the file "
                    "holds has that name", name));
        return generated{std::move(text_), std::move(names_)};
    }

private:
    std::optional<std::string> lookup(std::string_view name,
        const types::service* service) const {
        if (name.starts_with("service.")) {
            if (service == nullptr)
                return std::nullopt;
            const auto it = service->fields.find(name.substr(8));
            if (it == service->fields.end())
                return std::nullopt;
            return it->second;
        }
        if (const auto it = computed_.find(name); it != computed_.end())
            return it->second;
        if (const auto it = s_.inputs.find(name); it != s_.inputs.end())
            return it->second;
        return std::nullopt;
    }

    std::expected<std::string, std::string> value_of(const types::variable& v,
        const std::string& name, const resolver& resolve) {
        const auto existing = nonempty(s_.existing, name);
        const auto environment = [&] {
            const auto e = s_.environment ? s_.environment(name)
                                          : std::optional<std::string>{};
            return e.value_or(std::string{});
        };
        switch (v.kind) {
        case value_kind::value:
            return expand(v.argument, resolve);
        case value_kind::offset:
            return offset(v.argument, resolve);
        case value_kind::secret: {
            if (!existing.empty())
                return std::string(existing);
            std::size_t length = default_secret_length;
            if (!v.argument.empty())
                std::from_chars(v.argument.data(),
                    v.argument.data() + v.argument.size(), length);
            return s_.secret(length);
        }
        case value_kind::kept:
            if (!existing.empty())
                return std::string(existing);
            return expand(v.argument, resolve);
        case value_kind::inherited: {
            if (!existing.empty())
                return std::string(existing);
            if (auto e = environment(); !e.empty())
                return e;
            return expand(v.argument, resolve);
        }
        case value_kind::environment:
            if (auto e = environment(); !e.empty())
                return e;
            return expand(v.argument, resolve);
        case value_kind::version:
            return std::to_string(f_.current());
        case value_kind::file: {
            const auto path = expand(v.argument, resolve);
            if (!path)
                return path;
            const auto text = s_.read_file ? s_.read_file(*path)
                                           : std::optional<std::string>{};
            if (!text)
                return failure(std::format("cannot read {}", *path));
            std::string out = "\"";
            for (const auto line : lines_of(*text)) {
                out += line;
                out += "\\n";
            }
            return out + "\"";
        }
        case value_kind::carried:
            break;
        }
        return failure("carried variables have no single value");
    }

    std::expected<std::string, std::string> offset(std::string_view argument,
        const resolver& resolve) {
        long long sum = 0;
        for (const auto& term : list_of(argument)) {
            const auto t = expand(term, resolve);
            if (!t)
                return t;
            long long n = 0;
            const auto [end, ec] = std::from_chars(t->data(),
                t->data() + t->size(), n);
            if (ec != std::errc{} || end != t->data() + t->size())
                return failure(std::format("the term {} is \"{}\", not a "
                    "whole number", term, *t));
            sum += n;
        }
        return std::to_string(sum);
    }

    std::expected<void, std::string> blocks(const types::variable& v,
        const resolver& resolve) {
        const bool new_section = !v.section.empty() &&
            (!section_ || *section_ != v.section);
        section_ = v.section;
        for (const auto* block : {new_section ? &v.section : nullptr,
                 v.comment.empty() ? nullptr : &v.comment}) {
            if (block == nullptr)
                continue;
            const auto it = f_.comments.find(*block);
            if (it == f_.comments.end())
                return failure(std::format("no comment is named {}",
                    *block));
            for (const auto& line : it->second) {
                const auto text = expand(line, resolve);
                if (!text)
                    return failure(std::format("the comment {}: {}", *block,
                        text.error()));
                text_ += *text + "\n";
            }
        }
        return {};
    }

    std::expected<void, std::string> carried(const types::variable& v,
        const resolver& resolve) {
        const platform::text::regex pattern(v.argument);
        bool first = true;
        for (const auto& [name, value] : s_.existing) {
            if (value.empty() || !pattern.fullmatch(name) ||
                std::ranges::contains(names_, name))
                continue;
            if (first) {
                if (const auto r = blocks(v, resolve); !r)
                    return r;
                first = false;
            }
            text_ += name + "=" + value + "\n";
            names_.push_back(name);
        }
        return {};
    }

    const types::env_format& f_;
    const sources& s_;
    values computed_;
    std::set<std::string, std::less<>> used_;
    std::optional<std::string> section_;
    std::string text_;
    std::vector<std::string> names_;
};

/**
 * @brief The names of the live variables @p v stands for: one, or one a
 * service it applies to.
 */
std::vector<std::string> names_of(const types::variable& v,
    std::span<const types::service> services) {
    if (!v.per_service())
        return {v.name};
    std::vector<std::string> out;
    for (const auto& s : services) {
        if (!applies_to(v, s))
            continue;
        const auto name = expand(v.name, [&](std::string_view n)
            -> std::optional<std::string> {
            const auto it = s.fields.find(n.substr(8));
            if (!n.starts_with("service.") || it == s.fields.end())
                return std::nullopt;
            return it->second;
        });
        if (name)
            out.push_back(*name);
    }
    return out;
}

}

values read_env(std::string_view text) {
    values out;
    for (const auto line : lines_of(text)) {
        const auto first = line.find_first_not_of(" \t");
        if (line.empty() || (first != std::string_view::npos &&
                line[first] == '#'))
            continue;
        const auto equals = line.find('=');
        if (equals == std::string_view::npos || equals == 0)
            continue;
        out.emplace(std::string(line.substr(0, equals)),
            std::string(line.substr(equals + 1)));
    }
    return out;
}

std::vector<types::service> read_services(std::string_view source) {
    compass::org::types::tables tables;
    ingest::org::parse(source, tables);
    model::ops::outline o;
    model::ops::build_outline(source, compass::org::types::view(tables), o);
    std::vector<types::service> out;
    for (const auto n : model::ops::children(o, model::ops::node_id{0})) {
        if (!model::ops::property(o, n, "replicas").found)
            continue;
        types::service s;
        for (const auto& [key, value] : model::ops::properties(o, n))
            s.fields.emplace(std::string(key), std::string(value));
        s.fields["name"] = std::string(o.titles[n.value]);
        out.push_back(std::move(s));
    }
    return out;
}

bool applies_to(const types::variable& v, const types::service& s) {
    for (const auto& field : fields_used(v))
        if (!s.fields.contains(field))
            return false;
    if (v.services.empty())
        return true;
    const auto name = s.fields.find("name");
    const bool listed = name != s.fields.end() &&
        std::ranges::contains(v.services, name->second);
    return listed != v.excludes_services;
}

std::expected<generated, std::string> generate(const types::env_format& f,
    std::span<const types::service> services, const sources& s) {
    for (const auto& i : f.inputs)
        if (!s.inputs.contains(i.name))
            return failure(std::format("the input {} is not given; pass "
                "--input {}=<value>: {}", i.name, i.name, i.purpose));
    for (const auto& [name, value] : s.inputs)
        if (!std::ranges::contains(builtin_inputs, name) &&
            !std::ranges::contains(f.inputs, name, &types::input::name))
            return failure(std::format("{} declares no input {}", f.source,
                name));
    writer w(f, s);
    const auto& rows = f.variables;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].removed != 0)
            continue;
        if (!rows[i].per_service()) {
            if (const auto r = w.row(rows[i], nullptr); !r)
                return failure(r.error());
            continue;
        }
        std::vector<const types::variable*> group;
        auto j = i;
        for (; j < rows.size() &&
             (rows[j].removed != 0 || rows[j].per_service()); ++j)
            if (rows[j].removed == 0)
                group.push_back(&rows[j]);
        for (const auto& service : services)
            for (const auto* v : group)
                if (applies_to(*v, service))
                    if (const auto r = w.row(*v, &service); !r)
                        return failure(r.error());
        i = j - 1;
    }
    return w.finish();
}

status_report status_of(const types::env_format& f,
    std::span<const types::service> services, const values& existing) {
    status_report out;
    out.current = f.current();
    if (const auto it = existing.find(f.version_variable);
        it != existing.end()) {
        std::uint32_t n = 0;
        const auto& v = it->second;
        const auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(),
            n);
        if (ec == std::errc{} && end == v.data() + v.size())
            out.version = n;
    }
    std::set<std::string, std::less<>> allowed;
    std::set<std::string, std::less<>> removed;
    std::vector<platform::text::regex> carried;
    for (const auto& v : f.variables) {
        if (v.kind == value_kind::carried) {
            if (v.removed == 0)
                carried.emplace_back(v.argument);
            continue;
        }
        for (auto& name : names_of(v, services)) {
            if (v.removed != 0) {
                removed.insert(std::move(name));
                continue;
            }
            if (v.when.empty() && !existing.contains(name) &&
                !std::ranges::contains(out.missing, name))
                out.missing.push_back(name);
            allowed.insert(std::move(name));
        }
    }
    for (const auto& [name, value] : existing) {
        if (allowed.contains(name))
            continue;
        if (removed.contains(name))
            out.removed.push_back(name);
        else if (!std::ranges::any_of(carried, [&](const auto& r) {
                     return r.fullmatch(name).has_value();
                 }))
            out.undeclared.push_back(name);
    }
    return out;
}

}
