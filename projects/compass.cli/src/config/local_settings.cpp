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
#include "compass.cli/config/local_settings.hpp"

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include "compass.ingest/org/parser.hpp"
#include "compass.org/ops/document_fields.hpp"
#include "compass.platform/filesystem/new_file.hpp"
#include "compass.store/io/location.hpp"

namespace compass::cli::config {

namespace {

namespace fs = std::filesystem;

constexpr std::string_view settings_file = "compass/local.org";

constexpr std::string_view new_file_header =
    "#+title: compass local settings\n"
    "#+description: This machine's settings for compass in this "
    "repository; never committed.\n\n";

/**
 * @brief Sets into @p s what the file @p path holds; a missing file holds
 * nothing.
 */
std::expected<void, std::string> read_into(const fs::path& path,
    local_settings& s) {
    std::error_code ec;
    if (!fs::exists(path, ec))
        return {};
    std::ifstream is(path, std::ios::binary);
    if (!is)
        return std::unexpected(std::format("{}: cannot be read; fix: make "
            "it readable", path.generic_string()));
    const std::string text{std::istreambuf_iterator<char>(is), {}};
    org::types::tables t;
    ingest::org::parse(text, t);
    const auto v = org::types::view(t);
    const auto keyword = [&](std::string_view key) -> std::optional<std::string> {
        const auto k = org::ops::find_keyword(v, key);
        if (k.is_none())
            return std::nullopt;
        return std::string(org::ops::trimmed(text, v.keywords.values[k])
                               .of(text));
    };
    if (auto database = keyword("database")) {
        s.database = std::move(*database);
        s.database_file = path;
    }
    if (auto environment = keyword("environment"))
        s.environment = std::move(*environment);
    return {};
}

bool sets(std::string_view line, std::string_view key) {
    if (!line.starts_with("#+") || line.size() < key.size() + 3)
        return false;
    for (std::size_t i = 0; i < key.size(); ++i) {
        const auto c = line[2 + i];
        if (std::tolower(static_cast<unsigned char>(c)) != key[i])
            return false;
    }
    return line[2 + key.size()] == ':';
}

}

fs::path repository_settings_path(const fs::path& checkout) {
    return store::io::git_common_dir(checkout) / settings_file;
}

fs::path worktree_settings_path(const fs::path& checkout) {
    return store::io::git_dir(checkout) / settings_file;
}

std::expected<local_settings, std::string> read_local_settings(
    const fs::path& checkout) {
    local_settings s;
    fs::path repository;
    fs::path worktree;
    try {
        repository = repository_settings_path(checkout);
        worktree = worktree_settings_path(checkout);
    } catch (const std::runtime_error&) {
        return s;
    }
    if (auto r = read_into(repository, s); !r)
        return std::unexpected(r.error());
    if (worktree != repository)
        if (auto r = read_into(worktree, s); !r)
            return std::unexpected(r.error());
    return s;
}

void write_local_setting(const fs::path& file, std::string_view key,
    std::string_view value) {
    std::string text;
    if (std::ifstream is(file, std::ios::binary); is)
        text.assign(std::istreambuf_iterator<char>(is), {});
    else
        text = new_file_header;
    const auto line = std::format("#+{}: {}", key, value);
    std::string out;
    bool replaced = false;
    std::size_t at = 0;
    while (at < text.size()) {
        const auto end = std::min(text.find('\n', at), text.size());
        const auto current = std::string_view(text).substr(at, end - at);
        if (!replaced && sets(current, key)) {
            out += line;
            replaced = true;
        } else {
            out += current;
        }
        out += '\n';
        at = end + 1;
    }
    if (!replaced)
        out += line + "\n";
    platform::filesystem::update_file(file, out, true);
}

std::string environment_name(const fs::path& checkout,
    const local_settings& s, std::string_view given) {
    if (!given.empty())
        return std::string(given);
    if (!s.environment.empty())
        return s.environment;
    auto name = checkout.filename();
    if (name.empty())
        name = checkout.parent_path().filename();
    return name.string();
}

}
