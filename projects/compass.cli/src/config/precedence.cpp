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
#include "compass.cli/config/precedence.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace compass::cli::config {

namespace {

/**
 * @brief Where @p s was written, to start a problem with: the file and
 * line, or the --set that wrote it.
 */
std::string place_of(const types::setting& s, const std::string& file) {
    if (s.from.kind == types::origin_kind::file)
        return std::format("{}:{}", file, s.from.line);
    return std::format("--set {}.{}={}", s.codec, s.key, s.value);
}

std::string names_of(std::span<const std::string> names) {
    std::string r;
    for (const auto& n : names)
        r += (r.empty() ? "" : ", ") + n;
    return r.empty() ? "none" : r;
}

void apply(types::configuration& c, const types::setting& s,
    const std::string& file) {
    const auto codec = std::ranges::find(c.codecs, s.codec,
        &types::codec_configuration::name);
    if (codec == c.codecs.end()) {
        std::vector<std::string> names;
        for (const auto& known : c.codecs)
            names.push_back(known.name);
        c.problems.push_back(std::format(
            "{}: no codec is named '{}'; fix: use one of {}",
            place_of(s, file), s.codec, names_of(names)));
        return;
    }
    if (s.key == codec::types::enabled_key) {
        if (s.value == "true" || s.value == "false")
            codec->enabled = {s.value, s.from};
        else
            c.problems.push_back(std::format(
                "{}: {}.enabled is '{}'; fix: set it to true or false",
                place_of(s, file), s.codec, s.value));
        return;
    }
    const auto key = std::ranges::find(codec->keys, s.key);
    if (key == codec->keys.end()) {
        c.problems.push_back(std::format(
            "{}: codec '{}' has no option '{}'; fix: use one of {}",
            place_of(s, file), s.codec, s.key, names_of(codec->keys)));
        return;
    }
    codec->options[key - codec->keys.begin()] = {s.value, s.from};
}

void apply(types::sourced_path& to,
    const std::optional<types::sourced_path>& from) {
    if (from)
        to = *from;
}

}

std::filesystem::path absolute_under(const std::filesystem::path& base,
    const std::filesystem::path& p) {
    auto r = (base / p).lexically_normal();
    if (!r.has_filename() && r.has_relative_path())
        r = r.parent_path();
    return r;
}

types::configuration resolve(
    std::span<const codec::types::descriptor> catalogue,
    const std::filesystem::path& checkout, const std::filesystem::path& file,
    const types::layer& from_file, const types::layer& command_line) {
    types::configuration c;
    c.file = file;
    c.output = {std::filesystem::path(default_output), {}};
    c.meta = {std::filesystem::path(default_meta), {}};
    for (const auto& d : catalogue) {
        auto& codec = c.codecs.emplace_back();
        codec.name = d.name;
        codec.enabled = {d.enabled_by_default ? "true" : "false", {}};
        for (const auto& o : d.options) {
            codec.keys.emplace_back(o.key);
            codec.options.push_back({std::string(o.default_value), {}});
        }
    }

    const auto label = file.lexically_relative(checkout).string();
    for (const auto* layer : {&from_file, &command_line}) {
        apply(c.output, layer->output);
        apply(c.meta, layer->meta);
        for (const auto& s : layer->settings)
            apply(c, s, label);
    }
    c.output.value = absolute_under(checkout, c.output.value);
    c.meta.value = absolute_under(checkout, c.meta.value);
    return c;
}

codec::types::configuration settings_of(const types::configuration& c) {
    codec::types::configuration r;
    for (const auto& codec : c.codecs) {
        r.set(codec.name, codec::types::enabled_key, codec.enabled.value);
        for (std::size_t i = 0; i < codec.keys.size(); ++i)
            r.set(codec.name, codec.keys[i], codec.options[i].value);
    }
    return r;
}

}
