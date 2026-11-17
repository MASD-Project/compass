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
#ifndef COMPASS_CLI_IO_HOOK_PAYLOAD_HPP
#define COMPASS_CLI_IO_HOOK_PAYLOAD_HPP

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace compass::cli::io {

/**
 * @brief The edited file a Claude Code PostToolUse payload names: the
 * tool that edited it, its absolute, lexically normal path, and whether
 * the edit may have created it. Edit and MultiEdit only change a file
 * that exists; Write says it updated one with a tool_response type of
 * update. Any other payload may have created the file.
 */
struct hook_payload {
    std::string tool;
    std::filesystem::path file;
    bool created = true;
};

/**
 * @brief The payload Claude Code writes to a PostToolUse hook's standard
 * input: a JSON object whose tool_name names the tool, whose
 * tool_input.file_path names the file, and whose cwd a relative path is
 * taken against.
 * @return the error, when @p json is not such an object or names no file.
 */
[[nodiscard]] std::expected<hook_payload, std::string> parse_hook_payload(
    std::string_view json);

/**
 * @brief The checkout @p file belongs to: the nearest directory above it
 * that holds a .git entry, or nothing.
 */
[[nodiscard]] std::optional<std::filesystem::path> checkout_of(
    const std::filesystem::path& file);

}

#endif
