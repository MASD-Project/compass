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
#include "compass.cli/io/version.hpp"

#include <chrono>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <system_error>
#include "compass.codec/ops/action_cache.hpp"
#include "compass.codec/ops/dependencies.hpp"
#include "compass.codec/ops/format_cache.hpp"
#include "compass.codec/ops/outputs.hpp"
#include "compass.platform/filesystem/file_status.hpp"
#include "compass.store/ops/pack.hpp"
#include "compass.store/ops/worktree_index.hpp"

namespace compass::cli::io {

namespace {

/**
 * @brief What the manifest says about the build: the commit, short and
 * full, its committer time, whether the tree was dirty, the branch, and
 * when the install script installed it. Every field but the commits and
 * the dirty flag may be empty.
 */
struct build_facts {
    std::string commit;
    std::string commit_full;
    std::string commit_time;
    bool dirty = false;
    std::string branch;
    std::string installed;
};

/**
 * @brief The facts of the manifest at @p path; nothing when it cannot be
 * read, is not format 1, or lacks either commit or a dirty flag of true or
 * false.
 */
std::optional<build_facts> read_manifest(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in)
        return std::nullopt;
    std::map<std::string, std::string, std::less<>> fields;
    for (std::string line; std::getline(in, line);) {
        const auto equals = line.find('=');
        if (equals != std::string::npos && equals != 0)
            fields.emplace(line.substr(0, equals), line.substr(equals + 1));
    }
    const auto field = [&](std::string_view key) {
        const auto i = fields.find(key);
        return i == fields.end() ? std::string() : i->second;
    };
    build_facts r{.commit = field("commit"),
        .commit_full = field("commit_full"),
        .commit_time = field("commit_time"),
        .dirty = field("dirty") == "true",
        .branch = field("branch"),
        .installed = field("installed")};
    const auto dirty = field("dirty");
    const auto named = [](const std::string& c) {
        return !c.empty() && c != "unknown";
    };
    if (field("format") != "1" || !named(r.commit) || !named(r.commit_full) ||
        (dirty != "true" && dirty != "false"))
        return std::nullopt;
    return r;
}

/**
 * @brief The last write time of @p executable in UTC, as
 * YYYY-MM-DDThh:mm:ssZ; empty when it cannot be read.
 */
std::string build_time(const std::filesystem::path& executable) {
    const auto status = platform::filesystem::regular_file_status(executable);
    if (!status)
        return {};
    const std::chrono::sys_seconds written{
        std::chrono::seconds(status->modified.seconds)};
    return std::format("{:%FT%TZ}", written);
}

std::string or_unknown(const std::string& s) {
    return s.empty() ? "unknown" : s;
}

}

std::string version_text(const std::filesystem::path& executable) {
    const auto facts = executable.empty()
        ? std::nullopt
        : read_manifest(executable.parent_path() / manifest_name);
    const auto built = executable.empty() ? "" : build_time(executable);
    const build_facts none;

    const auto& f = facts ? *facts : none;
    std::string r = "compass " COMPASS_VERSION "\n";
    r += "commit: " +
        (facts ? f.commit_full + " (" + f.commit + ")" : "unknown") + "\n";
    r += "dirty: " + std::string(facts ? (f.dirty ? "yes" : "no") : "unknown") +
        "\n";
    r += "branch: " + or_unknown(f.branch) + "\n";
    r += "committed: " + or_unknown(f.commit_time) + "\n";
    r += "built: " + or_unknown(built) + "\n";
    r += "installed: " +
        (facts ? (f.installed.empty() ? "no" : f.installed) : "unknown") + "\n";
    r += "store formats: store layout " +
        std::to_string(store::ops::store_layout_version) + ", packs " +
        std::to_string(store::ops::pack_version) + ", dependency records " +
        std::to_string(codec::ops::dependency_record_version) + ", actions " +
        std::to_string(codec::ops::action_format_version) +
        ", written outputs " +
        std::to_string(codec::ops::written_outputs_version) +
        ", format cache " + std::to_string(codec::ops::format_cache_version) +
        ", worktree index " +
        std::to_string(store::ops::worktree_index_version) + "\n";
    return r;
}

}
