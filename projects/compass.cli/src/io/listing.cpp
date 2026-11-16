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
#include "compass.cli/io/listing.hpp"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <sys/stat.h>
#include "compass.platform/process/run_program.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/git_object.hpp"

namespace compass::cli::io {

namespace {

/**
 * @brief Runs git in @p checkout with @p args to completion.
 */
platform::process::program_result git(const std::filesystem::path& checkout,
    std::vector<std::string> args) {
    args.insert(args.begin(), {"git", "-C", checkout.string()});
    return platform::process::run_program({std::move(args), {}, {}});
}

/**
 * @brief Runs git in @p checkout with @p args and returns its standard
 * output.
 */
std::string run_git(const std::filesystem::path& checkout,
    std::vector<std::string> args) {
    const auto command = args.front();
    auto r = git(checkout, std::move(args));
    if (r.exit_code != 0)
        throw std::runtime_error("git " + command + " failed for " +
            checkout.string());
    return std::move(r.output);
}

/**
 * @brief The commit ID @p command printed on its first line.
 */
store::types::object_id id_printed(std::string out,
    std::string_view command) {
    while (!out.empty() && out.back() == '\n')
        out.pop_back();
    const auto id = store::ops::from_hex(out);
    if (!id)
        throw std::runtime_error("git " + std::string(command) +
            " printed no commit ID");
    return *id;
}

/**
 * @brief The NUL-terminated fields of what a git -z command printed.
 */
std::vector<std::string_view> fields_of(std::string_view raw) {
    std::vector<std::string_view> out;
    while (!raw.empty()) {
        const auto end = raw.find('\0');
        out.push_back(raw.substr(0, end));
        raw = end == std::string_view::npos ? std::string_view{}
                                            : raw.substr(end + 1);
    }
    return out;
}

bool is_plain_file(const std::filesystem::path& path) {
    struct stat st {};
    return ::lstat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

}

std::vector<std::string> list_files(const std::filesystem::path& checkout) {
    const auto raw = run_git(checkout,
        {"ls-files", "-z", "--cached", "--others", "--exclude-standard"});
    std::vector<std::string> out;
    for (const auto path : fields_of(raw))
        out.emplace_back(path);
    // git lists a path with conflicts once per stage.
    std::ranges::sort(out);
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

store::types::object_id resolve_commit(const std::filesystem::path& checkout,
    std::string_view revision) {
    std::string out;
    try {
        out = run_git(checkout, {"rev-parse", "--verify", "--quiet",
            "--end-of-options", std::string(revision) + "^{commit}"});
    } catch (const std::runtime_error&) {
        throw std::runtime_error("no commit is named " +
            std::string(revision) + " in " + checkout.string());
    }
    return id_printed(std::move(out), "rev-parse");
}

named_commit resolve_base(const std::filesystem::path& checkout,
    std::string_view base) {
    if (!base.empty())
        return {std::string(base), resolve_commit(checkout, base)};
    for (const std::string_view fallback : {"@{upstream}", "origin/main"}) {
        try {
            return {std::string(fallback), resolve_commit(checkout, fallback)};
        } catch (const std::runtime_error&) {
        }
    }
    throw std::runtime_error("no base to ratchet against: the branch has no "
        "upstream and there is no origin/main; name one with --ratchet "
        "<base>");
}

store::types::object_id merge_base(const std::filesystem::path& checkout,
    const store::types::object_id& a, const store::types::object_id& b) {
    auto r = git(checkout, {"merge-base", store::ops::to_hex(a),
        store::ops::to_hex(b)});
    if (r.exit_code == 1)
        throw std::runtime_error(store::ops::to_hex(a) + " and " +
            store::ops::to_hex(b) + " share no history");
    if (r.exit_code != 0)
        throw std::runtime_error("git merge-base failed for " +
            checkout.string());
    return id_printed(std::move(r.output), "merge-base");
}

std::vector<types::rename> similar_renames(
    const std::filesystem::path& checkout,
    const store::types::object_id& from, const store::types::object_id& to) {
    const auto raw = run_git(checkout, {"diff", "-z", "--name-status", "-M",
        "--diff-filter=R", store::ops::to_hex(from), store::ops::to_hex(to)});
    const auto fields = fields_of(raw);
    std::vector<types::rename> out;
    for (std::size_t i = 0; i + 3 <= fields.size(); i += 3)
        out.push_back({std::string(fields[i + 1]), std::string(fields[i + 2])});
    return out;
}

std::vector<std::string_view> org_files(const std::filesystem::path& checkout,
    const std::vector<std::string>& files) {
    std::vector<std::string_view> out;
    for (const auto& path : files) {
        if (path.ends_with(".org") && is_plain_file(checkout / path))
            out.emplace_back(path);
    }
    return out;
}

}
