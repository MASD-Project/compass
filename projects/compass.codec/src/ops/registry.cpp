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
#include "compass.codec/ops/registry.hpp"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string_view>
#include "compass.org/ops/type_lookup.hpp"

namespace compass::codec::ops {

namespace {

namespace ot = compass::org::types;

types::codec_id find_codec(const types::registry& r,
    std::string_view name) noexcept {
    for (std::uint32_t c = 0; c < r.size(); ++c) {
        if (r.codecs[c].name == name)
            return {c};
    }
    return types::codec_id::none();
}

/**
 * @brief The slot of @p type among the flags of one codec: its row, or the
 * last slot for none.
 */
std::size_t slot(const types::registry& r, ot::type_id type) noexcept {
    return type.is_none() ? r.type_slots - 1 : type.value;
}

void add(types::registry& r, const types::descriptor& d,
    const ot::type_registry& registry) {
    if (!find_codec(r, d.name).is_none()) {
        r.problems.push_back(
            std::format("codec '{}' is registered twice", d.name));
        return;
    }
    r.codecs.push_back(d);
    r.enabled.push_back(d.enabled_by_default ? 1 : 0);
    r.option_firsts.push_back(
        static_cast<std::uint32_t>(r.option_values.size()));
    for (const auto& o : d.options)
        r.option_values.emplace_back(o.default_value);

    const auto first = r.reads.size();
    r.reads.resize(first + r.type_slots,
        d.reads.scope == types::input_scope::listed ? 0 : 1);
    if (d.reads.scope == types::input_scope::typed)
        r.reads.back() = 0;
    for (const auto name : d.reads.types) {
        const auto type = org::ops::find_type(registry, name);
        if (type.is_none())
            r.problems.push_back(std::format(
                "codec '{}' reads type '{}', which is not registered",
                d.name, name));
        else
            r.reads[first + type.value] = 1;
    }
}

void apply(types::registry& r, std::string_view codec, std::string_view key,
    std::string_view value) {
    const auto c = find_codec(r, codec);
    if (c.is_none()) {
        r.problems.push_back(std::format(
            "setting '{}.{}' names a codec that is not registered", codec,
            key));
        return;
    }
    if (key == types::enabled_key) {
        if (value == "true" || value == "false")
            r.enabled[c.value] = value == "true" ? 1 : 0;
        else
            r.problems.push_back(std::format(
                "setting '{}.{}' is '{}', not true or false", codec, key,
                value));
        return;
    }
    const auto options = r.codecs[c.value].options;
    for (std::size_t i = 0; i < options.size(); ++i) {
        if (options[i].key == key) {
            r.option_values[r.option_firsts[c.value] + i] = value;
            return;
        }
    }
    r.problems.push_back(std::format(
        "setting '{}.{}' names an option the codec does not declare", codec,
        key));
}

}

types::registry make_registry(std::span<const types::descriptor> codecs,
    const ot::type_registry& types, const types::configuration& settings) {
    types::registry r;
    r.type_slots = types.types.size() + 1;
    for (const auto& d : codecs)
        add(r, d, types);
    for (std::size_t i = 0; i < settings.size(); ++i)
        apply(r, settings.codecs[i], settings.keys[i], settings.values[i]);
    return r;
}

bool reads(const types::registry& r, types::codec_id codec,
    ot::type_id type) noexcept {
    return r.reads[codec.value * r.type_slots + slot(r, type)] != 0;
}

std::span<const std::string> option_values(const types::registry& r,
    types::codec_id codec) noexcept {
    return std::span<const std::string>(r.option_values).subspan(
        r.option_firsts[codec.value], r.codecs[codec.value].options.size());
}

}
