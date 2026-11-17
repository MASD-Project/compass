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
#include "compass.cli/io/hook_payload.hpp"

#include <stdexcept>
#include <system_error>
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"

namespace compass::cli::io {

namespace {

namespace mt = compass::model::types;

/**
 * @brief The member @p key of the object @p object, or none when it has
 * no such member or is not an object.
 */
mt::value_id member(const mt::model_tables& t, mt::value_id object,
    std::string_view key) {
    if (object.is_none() || t.values.kinds[object] != mt::value_kind::object)
        return mt::value_id::none();
    for (auto v = t.values.firsts[object]; !v.is_none();
         v = t.values.nexts[v]) {
        if (mt::text_of(t.texts, t.values.keys[v]) == key)
            return v;
    }
    return mt::value_id::none();
}

std::optional<std::string_view> string_of(const mt::model_tables& t,
    mt::value_id v) {
    if (v.is_none() || t.values.kinds[v] != mt::value_kind::string)
        return std::nullopt;
    return mt::text_of(t.texts,
        mt::text_id{static_cast<std::uint32_t>(t.values.scalars[v])});
}

}

std::expected<hook_payload, std::string> parse_hook_payload(
    std::string_view json) {
    mt::model_tables t;
    compass::model::ops::builder b(t);
    mt::value_id root;
    try {
        root = compass::model::io::read_json(b, json);
    } catch (const std::runtime_error& e) {
        return std::unexpected(std::string("the hook payload is not JSON: ") +
            e.what());
    }
    const auto file = string_of(t, member(t, member(t, root, "tool_input"),
        "file_path"));
    if (!file || file->empty())
        return std::unexpected(
            std::string("the hook payload names no tool_input.file_path"));
    hook_payload p;
    p.tool = string_of(t, member(t, root, "tool_name")).value_or("");
    std::filesystem::path path(*file);
    if (path.is_relative()) {
        const auto cwd = string_of(t, member(t, root, "cwd"));
        if (!cwd || cwd->empty())
            return std::unexpected(std::string("the hook payload names a "
                "relative file_path and no cwd"));
        path = std::filesystem::path(*cwd) / path;
    }
    p.file = path.lexically_normal();
    const auto type = string_of(t, member(t, member(t, root, "tool_response"),
        "type"));
    p.created = !(p.tool == "Edit" || p.tool == "MultiEdit" ||
        (p.tool == "Write" && type == "update"));
    return p;
}

std::optional<std::filesystem::path> checkout_of(
    const std::filesystem::path& file) {
    std::error_code ec;
    for (auto dir = file.parent_path(); !dir.empty();
         dir = dir.parent_path()) {
        if (std::filesystem::exists(dir / ".git", ec))
            return dir;
        if (dir == dir.root_path())
            break;
    }
    return std::nullopt;
}

}
