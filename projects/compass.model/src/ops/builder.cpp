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
#include "compass.model/ops/builder.hpp"

namespace compass::model::ops {

using types::text_id;
using types::value_id;
using types::value_kind;

text_id builder::intern(std::string_view s) {
    const auto it = interned_.find(s);
    if (it != interned_.end())
        return it->second;
    auto& texts = t_->texts;
    const text_id id{static_cast<std::uint32_t>(texts.size())};
    texts.firsts.values.push_back(
        static_cast<std::uint32_t>(texts.bytes.size()));
    texts.bytes.insert(texts.bytes.end(), s.begin(), s.end());
    interned_.emplace(std::string(s), id);
    return id;
}

value_id builder::add_row(value_kind kind, text_id key, std::int64_t scalar) {
    auto& v = t_->values;
    const value_id id{static_cast<std::uint32_t>(v.size())};
    v.kinds.values.push_back(kind);
    v.keys.values.push_back(key);
    v.scalars.values.push_back(scalar);
    v.firsts.values.push_back(value_id::none());
    v.lasts.values.push_back(value_id::none());
    v.nexts.values.push_back(value_id::none());
    return id;
}

void builder::link(value_id parent, value_id child) {
    auto& v = t_->values;
    if (v.lasts[parent].is_none())
        v.firsts[parent] = child;
    else
        v.nexts[v.lasts[parent]] = child;
    v.lasts[parent] = child;
}

value_id builder::add_model(std::string_view path, types::model_type t) {
    const auto root = add_row(value_kind::object, text_id::none(), 0);
    auto& m = t_->models;
    m.paths.values.push_back(intern(path));
    m.types.values.push_back(t);
    m.roots.values.push_back(root);
    return root;
}

value_id builder::find(value_id parent, std::string_view key) const {
    const auto& v = t_->values;
    for (auto c = v.firsts[parent]; !c.is_none(); c = v.nexts[c]) {
        if (types::text_of(t_->texts, v.keys[c]) == key)
            return c;
    }
    return value_id::none();
}

value_id builder::slot(value_id parent, std::string_view key, value_kind kind,
    std::int64_t scalar) {
    auto& v = t_->values;
    const auto existing = find(parent, key);
    if (existing.is_none()) {
        const auto child = add_row(kind, intern(key), scalar);
        link(parent, child);
        return child;
    }
    v.kinds[existing] = kind;
    v.scalars[existing] = scalar;
    v.firsts[existing] = value_id::none();
    v.lasts[existing] = value_id::none();
    return existing;
}

value_id builder::object(value_id parent, std::string_view key) {
    return slot(parent, key, value_kind::object, 0);
}

value_id builder::list(value_id parent, std::string_view key) {
    return slot(parent, key, value_kind::list, 0);
}

void builder::set(value_id parent, std::string_view key,
    std::string_view value) {
    const auto text = intern(value);
    slot(parent, key, value_kind::string, text.value);
}

void builder::set(value_id parent, std::string_view key, bool value) {
    slot(parent, key, value_kind::boolean, value ? 1 : 0);
}

void builder::set_integer(value_id parent, std::string_view key,
    std::int64_t value) {
    slot(parent, key, value_kind::integer, value);
}

void builder::set_null(value_id parent, std::string_view key) {
    slot(parent, key, value_kind::null, 0);
}

value_id builder::copy(value_id value, text_id key) {
    const auto kind = t_->values.kinds[value];
    const auto scalar = t_->values.scalars[value];
    const auto out = add_row(kind, key, scalar);
    for (auto c = t_->values.firsts[value]; !c.is_none();
         c = t_->values.nexts[c]) {
        const auto child = copy(c, t_->values.keys[c]);
        link(out, child);
    }
    return out;
}

void builder::set_copy(value_id parent, std::string_view key, value_id value) {
    remove(parent, key);
    const auto k = intern(key);
    const auto child = copy(value, k);
    link(parent, child);
}

void builder::remove(value_id parent, std::string_view key) {
    auto& v = t_->values;
    auto previous = value_id::none();
    for (auto c = v.firsts[parent]; !c.is_none(); c = v.nexts[c]) {
        if (types::text_of(t_->texts, v.keys[c]) != key) {
            previous = c;
            continue;
        }
        if (previous.is_none())
            v.firsts[parent] = v.nexts[c];
        else
            v.nexts[previous] = v.nexts[c];
        if (v.lasts[parent] == c)
            v.lasts[parent] = previous;
        return;
    }
}

value_id builder::append_object(value_id list) {
    const auto child = add_row(value_kind::object, text_id::none(), 0);
    link(list, child);
    return child;
}

value_id builder::append_list(value_id list) {
    const auto child = add_row(value_kind::list, text_id::none(), 0);
    link(list, child);
    return child;
}

void builder::append(value_id list, std::string_view value) {
    const auto text = intern(value);
    link(list, add_row(value_kind::string, text_id::none(), text.value));
}

void builder::append(value_id list, bool value) {
    link(list, add_row(value_kind::boolean, text_id::none(), value ? 1 : 0));
}

void builder::append_integer(value_id list, std::int64_t value) {
    link(list, add_row(value_kind::integer, text_id::none(), value));
}

void builder::append_copy(value_id list, value_id value) {
    const auto child = copy(value, text_id::none());
    link(list, child);
}

value_id builder::ensure_object(value_id parent, std::string_view key) {
    const auto existing = find(parent, key);
    return existing.is_none() ? object(parent, key) : existing;
}

value_id builder::ensure_list(value_id parent, std::string_view key) {
    const auto existing = find(parent, key);
    return existing.is_none() ? list(parent, key) : existing;
}

}
