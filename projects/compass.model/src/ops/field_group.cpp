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
#include <string>
#include "loaders.hpp"
#include "text.hpp"

namespace compass::model::ops {

namespace {

using types::value_id;

bool option_secret(std::string_view owner, std::string_view raw) {
    const auto value = to_lower(trim(raw));
    if (value.empty() || value == "nil" || value == "false" || value == "no")
        return false;
    if (value == "t" || value == "true" || value == "yes")
        return true;
    throw model_error(std::string(owner) + ": option sets :secret: to " +
        value);
}

/**
 * @brief options_struct: what the options header needs of a field group
 * that names its #+header:.
 */
void options_struct(load_context& c, value_id fg,
    const std::vector<value_id>& fields) {
    auto& b = c.b;
    const auto& o = c.o;
    const auto header = trim(frontmatter_or(o, "header"));
    if (header.empty())
        return;
    const auto component_include = frontmatter_or(o, "component_include");
    const auto component = frontmatter(o, "component");
    std::string include(component_include.empty()
        ? component.value : component_include);
    const auto subcomponent = frontmatter_or(o, "subcomponent");
    if (!subcomponent.empty() && component_include.empty()) {
        include = (component.found ? std::string(component.value)
            : std::string("None")) + "." + std::string(subcomponent);
    }
    auto guard = "ores." + include + "/" + std::string(header);
    for (auto& ch : guard) {
        const bool alnum = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9');
        ch = alnum ? (ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 32) : ch)
            : '_';
    }
    const auto owner = file_name(c.d);
    b.set(fg, "header", header);
    b.set(fg, "header_guard", guard);
    b.set(fg, "export_macro", trim(frontmatter_or(o, "export_macro")));
    b.set(fg, "has_printer",
        to_lower(trim(frontmatter_or(o, "printer"))) != "none");
    const auto list = b.list(fg, "options_fields");
    std::int64_t secrets = 0;
    for (const auto field : fields) {
        const auto item = b.append_copy(list, field);
        std::string description;
        if (const auto d = b.find(item, "description"); !d.is_none())
            description = text_value(b, d);
        const auto doc = b.list(item, "doc_lines");
        const auto lines = lines_of(description);
        for (std::size_t i = 0; i < lines.size(); ++i) {
            std::string line(lines[i]);
            while (!line.empty() && is_space(line.back()))
                line.pop_back();
            b.append(doc, i == 0 ? "@brief " + line : line);
        }
        b.set(item, "has_doc", !lines.empty());
        std::string secret;
        if (const auto s = b.find(item, "secret"); !s.is_none())
            secret = text_value(b, s);
        const bool is_secret = option_secret(owner, secret);
        b.set(item, "secret", is_secret);
        secrets += is_secret ? 1 : 0;
    }
    b.set_integer(fg, "options_secret_count", secrets);
    b.set(fg, "has_options_secrets", secrets != 0);
}

}

void load_field_group(load_context& c, value_id root) {
    auto& b = c.b;
    const auto& o = c.o;
    ensure_profile_binding(o);
    reject_junction_only_flags(o, "field group");
    const auto fg = b.object(root, "field_group");
    for (const auto key : {"product", "component", "subcomponent",
        "component_include", "entity_singular", "brief"}) {
        const auto r = frontmatter(o, key);
        if (r.found)
            b.set(fg, key, r.value);
    }
    const node_id document{0};
    const auto description = body(o, document);
    if (!description.empty())
        b.set(fg, "description", description);

    const auto ns = frontmatter(o, "namespace");
    const auto includes = section(o, document, "Includes");
    if (ns.found || !includes.is_none()) {
        const auto cpp = b.object(fg, "cpp");
        if (ns.found)
            b.set(cpp, "namespace", ns.value);
        if (!includes.is_none())
            includes_from_named_block(c, includes, b.list(cpp, "includes"));
    }

    const auto list = b.list(fg, "fields");
    std::vector<value_id> fields;
    for (const auto n : children(o, section(o, document, "Fields"))) {
        const auto entry = b.append_object(list);
        fields.push_back(entry);
        b.set(entry, "name", o.titles[n.value]);
        for (const auto& p : properties(o, n)) {
            const auto key = to_lower(p.key);
            if (key == "default_value")
                b.set(entry, key, p.value);
            else
                set_typed(b, entry, key, p.value);
        }
        const auto text = body(o, n);
        if (!text.empty())
            b.set(entry, "description", text);
    }
    const auto e = economic_declaration(b, file_name(c.d), fields, {});
    write_economic(b, fg, e, true);
    options_struct(c, fg, fields);
    collect_implementations(c, fg, "implementations");
}

}
