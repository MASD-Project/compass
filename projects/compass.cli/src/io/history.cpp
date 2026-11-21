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
#include "compass.cli/io/history.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <system_error>
#include "compass.cli/io/json_value.hpp"
#include "compass.cli/ops/blame.hpp"
#include "compass.cli/ops/provenance.hpp"
#include "compass.model/io/json.hpp"
#include "compass.model/ops/builder.hpp"
#include "compass.platform/process/environment.hpp"
#include "compass.platform/process/run_program.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::cli::io {

namespace {

constexpr char field_separator = '\x1f';

/**
 * @brief Runs git with @p args in @p checkout, its standard error
 * dropped, since blame asks git about paths and revisions that may not
 * exist and says so itself.
 */
platform::process::program_result quiet_git(
    const std::filesystem::path& checkout, std::vector<std::string> args) {
    std::vector<std::string> command{"git", "-C", checkout.generic_string()};
    command.insert(command.end(), std::make_move_iterator(args.begin()),
        std::make_move_iterator(args.end()));
    return platform::process::run_program({std::move(command), {}, {},
        platform::process::error_stream::discard});
}

std::string_view first_line(std::string_view text) {
    while (!text.empty() && text.front() == '\n')
        text.remove_prefix(1);
    return text.substr(0, text.find('\n'));
}

}

std::vector<types::branch_commit> branch_commits(
    const std::filesystem::path& checkout,
    const store::types::object_id& from, const store::types::object_id& to) {
    auto r = platform::process::run_program({{"git", "-C",
        checkout.generic_string(), "log", "--reverse", "-z",
        "--format=%H%x1f%P%x1f%B", store::ops::to_hex(from) + ".." +
        store::ops::to_hex(to)}, {}, {}});
    if (r.exit_code != 0)
        throw std::runtime_error("git log failed for " +
            checkout.generic_string());
    std::vector<types::branch_commit> out;
    std::string_view rest = r.output;
    while (!rest.empty()) {
        const auto end = rest.find('\0');
        const auto record = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{}
                                             : rest.substr(end + 1);
        const auto a = record.find(field_separator);
        const auto b = record.find(field_separator, a + 1);
        if (a == std::string_view::npos || b == std::string_view::npos)
            continue;
        const auto parents = record.substr(a + 1, b - a - 1);
        const auto message = record.substr(b + 1);
        out.push_back({.id = std::string(record.substr(0, a)),
            .parents = parents.empty() ? 0 : static_cast<std::size_t>(
                std::ranges::count(parents, ' ') + 1),
            .subject = std::string(first_line(message)),
            .trailers = ops::closing_trailers(message)});
    }
    return out;
}

std::string head_branch(const std::filesystem::path& checkout) {
    auto r = platform::process::run_program({{"git", "-C",
        checkout.generic_string(), "symbolic-ref", "--quiet", "--short",
        "HEAD"}, {}, {}});
    if (r.exit_code == 0) {
        while (!r.output.empty() && r.output.back() == '\n')
            r.output.pop_back();
        if (!r.output.empty())
            return std::move(r.output);
    }
    return platform::process::environment_variable("GITHUB_HEAD_REF")
        .value_or("");
}

std::expected<types::pull_request, std::string> parse_pull_request(
    std::string_view json) {
    namespace mt = compass::model::types;
    mt::model_tables t;
    compass::model::ops::builder b(t);
    mt::value_id root;
    try {
        root = compass::model::io::read_json(b, json);
    } catch (const std::runtime_error& e) {
        return std::unexpected(std::string("gh printed no JSON: ") +
            e.what());
    }
    const auto number = integer_of(t, member(t, root, "number"));
    const auto body = string_of(t, member(t, root, "body"));
    if (!number || *number <= 0 || !body)
        return std::unexpected(std::string("gh printed no pull request "
            "number and body"));
    return types::pull_request{
        .number = static_cast<std::uint64_t>(*number),
        .url = std::string(string_of(t, member(t, root, "url")).value_or("")),
        .body = std::string(*body)};
}

std::expected<types::pull_request, std::string> find_pull_request(
    const std::filesystem::path& checkout, std::string_view gh,
    std::string_view branch) {
    if (gh.empty())
        return std::unexpected(std::string("gh is off, as ") +
            std::string(ops::provenance_codec) + ".gh is empty");
    std::vector<std::string> command{std::string(gh), "pr", "view"};
    if (!branch.empty())
        command.emplace_back(branch);
    command.insert(command.end(), {"--json", "number,body,url"});
    platform::process::program_result r;
    try {
        r = platform::process::run_program({std::move(command), checkout, {},
            platform::process::error_stream::merge});
    } catch (const std::system_error& e) {
        if (e.code() == std::errc::no_such_file_or_directory)
            return std::unexpected(std::string(gh) + " is not installed");
        return std::unexpected(std::string("gh could not run: ") + e.what());
    }
    if (r.exit_code != 0) {
        const auto line = first_line(r.output);
        return std::unexpected(std::string(gh) + ": " +
            std::string(line.empty() ? "it failed" : line));
    }
    return parse_pull_request(r.output);
}

std::vector<std::string> git_lines(const std::filesystem::path& checkout,
    const std::vector<std::string>& args) {
    const auto r = quiet_git(checkout, args);
    std::vector<std::string> out;
    if (r.exit_code != 0)
        return out;
    std::string_view rest = r.output;
    while (!rest.empty()) {
        const auto end = rest.find('\n');
        out.emplace_back(rest.substr(0, end));
        rest = end == std::string_view::npos ? std::string_view{}
                                             : rest.substr(end + 1);
    }
    return out;
}

std::optional<types::commit_detail> read_commit(
    const std::filesystem::path& checkout, std::string_view id) {
    const auto r = quiet_git(checkout, {"log", "-1",
        "--format=%H%x1f%an%x1f%B", std::string(id), "--"});
    if (r.exit_code != 0)
        return std::nullopt;
    const std::string_view record = r.output;
    const auto a = record.find(field_separator);
    const auto b = record.find(field_separator, a + 1);
    if (a == std::string_view::npos || b == std::string_view::npos)
        return std::nullopt;
    const auto message = record.substr(b + 1);
    return types::commit_detail{.id = std::string(record.substr(0, a)),
        .author = std::string(record.substr(a + 1, b - a - 1)),
        .subject = std::string(first_line(message)),
        .trailers = ops::closing_trailers(message)};
}

bool is_ancestor(const std::filesystem::path& checkout,
    std::string_view ancestor, std::string_view descendant) {
    const auto r = quiet_git(checkout, {"rev-list", "-n", "1",
        std::string(descendant) + ".." + std::string(ancestor)});
    return r.exit_code == 0 && r.output.empty();
}

}
