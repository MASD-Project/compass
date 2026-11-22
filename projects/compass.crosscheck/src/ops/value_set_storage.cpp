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
#include "compass.crosscheck/ops/value_sets.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include "compass.cpp/ops/paths.hpp"
#include "compass.model/ops/entity_claim.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::ops::value_sets {

namespace {

namespace ct = compass::codec::types;
namespace cops = compass::codec::ops;
namespace pt = compass::platform::text;

void replace_all(std::string& text, std::string_view from,
    std::string_view to) {
    for (auto at = text.find(from); at != std::string::npos;
         at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
}

std::string listed(std::span<const std::string_view> names) {
    std::string out;
    for (const auto n : names)
        out += (out.empty() ? "" : ", ") + std::string(n);
    return out;
}

/**
 * @brief The models of @p entity: each file <anything>.<entity>.org
 * directly under a modeling/ directory of projects/. The listing is an
 * input of the unit, so a model moved, added or removed runs it again.
 */
std::vector<std::string_view> models_of(const ct::corpus_view& v,
    std::string_view entity) {
    auto out = v.list_files("projects/", "." + std::string(entity) + ".org");
    std::erase_if(out, [](auto p) {
        return !compass::cpp::ops::parent_of(p).ends_with("/modeling");
    });
    return out;
}

}

const stored_entity& storage::operator[](role r) const {
    return entities[std::to_underlying(r)];
}

std::string storage::seed_path(role r) const {
    const auto& e = (*this)[r];
    return "projects/" + e.product + ".sql/populate/" + e.component + "/" +
        e.component + "_" + e.plural + "_populate.sql";
}

std::string storage::writer(role r) const {
    const auto& e = (*this)[r];
    return e.product + "_" + e.component + "_service";
}

std::string storage::function(std::string_view stem) const {
    const auto& e = (*this)[role::value_set];
    return e.product + "_" + e.component + "_" + std::string(stem) + "_fn";
}

std::string storage::fill(std::string_view text) const {
    std::string out(text);
    replace_all(out, "{{vocabulary_table}}", (*this)[role::vocabulary].table);
    replace_all(out, "{{element_map_column_table}}",
        (*this)[role::element_map_column].table);
    replace_all(out, "{{value_in_set_fn}}", function("value_in_set"));
    replace_all(out, "{{canonical_value_fn}}", function("canonical_value"));
    return out;
}

std::expected<storage, unreadable> read_storage(const ct::corpus_view& v,
    const cops::data_tables& data, std::string_view codec) {
    static const pt::regex component_re(R"(^#\+component:[ \t]*(\S+))",
        {.multiline = true});
    static const pt::regex product_re(R"(^:product:[ \t]*(\S+))",
        {.multiline = true});
    const auto wrong = [&](std::uint32_t line, std::string_view what) {
        return std::unexpected(unreadable{data.source, std::format(
            "{}:{}: {}, in the table \"{}\" {} reads", data.source, line,
            what, entities_table.name, codec)});
    };
    const auto* t = data.find(entities_table.name);
    if (t == nullptr)
        return wrong(0, "there is no such table");
    const auto role_at = t->column("role");
    const auto entity_at = t->column("entity");
    std::array<std::optional<std::size_t>, role_names.size()> row_of;
    for (std::size_t r = 0; r < t->rows.size(); ++r) {
        const auto& name = t->rows[r][role_at];
        const auto k = static_cast<std::size_t>(
            std::ranges::find(role_names, name) - role_names.begin());
        if (k == role_names.size())
            return wrong(t->row_lines[r], "the role '" + name + "' is none "
                "of " + listed(role_names));
        if (row_of[k])
            return wrong(t->row_lines[r], "the role " + name + " is stated "
                "twice");
        row_of[k] = r;
    }
    storage out;
    for (std::size_t k = 0; k < role_names.size(); ++k) {
        if (!row_of[k])
            return wrong(t->line, "no row names the entity of the role " +
                std::string(role_names[k]));
        const auto line = t->row_lines[*row_of[k]];
        const auto& entity = t->rows[*row_of[k]][entity_at];
        const auto models = models_of(v, entity);
        if (models.empty())
            return wrong(line, "no modeling/ directory holds a model of the "
                "entity " + entity);
        if (models.size() > 1)
            return wrong(line, "the entity " + entity + " has " +
                std::to_string(models.size()) + " models: " + listed(models));
        const std::string path(models.front());
        const auto text = v.read_file(path).value_or("");
        const auto claim = compass::model::ops::entity_claim_of(text);
        const auto component = component_re.search(text);
        const auto product = product_re.search(text);
        if (!claim || !component || !product)
            return wrong(line, path + " states no " + (!claim
                ? "#+entity_singular: or :tablename:"
                : !component ? "#+component:" : ":product:"));
        auto& e = out.entities[k];
        e.product = std::string(product->group(1));
        e.component = std::string(component->group(1));
        e.table = claim->table;
        e.plural = claim->entity_plural;
    }
    return out;
}

}
