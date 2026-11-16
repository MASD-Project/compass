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
#include "compass.bench/ops/search_paths.hpp"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace compass::bench::ops {

namespace {

std::string normal(const std::filesystem::path& p) {
    auto s = p.lexically_normal().generic_string();
    while (s.size() > 1 && s.back() == '/')
        s.pop_back();
    return s;
}

bool is_under(std::string_view path, std::string_view root) {
    return path == root ||
        (path.size() > root.size() && path.starts_with(root) &&
            (root == "/" || path[root.size()] == '/'));
}

std::string_view relative_to(std::string_view path, std::string_view root) {
    if (path.size() == root.size())
        return {};
    return path.substr(root == "/" ? 1 : root.size() + 1);
}

std::string_view parent_of(std::string_view relative) {
    const auto slash = relative.rfind('/');
    return slash == std::string_view::npos ? std::string_view{}
                                           : relative.substr(0, slash);
}

/**
 * @brief The directory ids one command searches, by category.
 */
struct command_directories {
    std::vector<std::uint32_t> quote;
    std::vector<std::uint32_t> user;
    std::vector<std::uint32_t> system;
};

class builder final {
public:
    builder(const std::filesystem::path& source_root,
        std::span<const std::string> implicit_system)
        : root_(normal(source_root)) {
        for (const auto& d : implicit_system)
            implicit_.push_back(intern(normal(d), false));
    }

    void add(const types::compile_command& c) {
        const auto build = normal(c.directory);
        const bool separate_build = build != root_;
        command_directories dirs;
        const auto& args = c.arguments;
        for (std::size_t i = 1; i < args.size(); ++i) {
            const std::string_view a = args[i];
            std::vector<std::uint32_t>* list = nullptr;
            std::string_view value;
            for (const auto& [flag, target] :
                {std::pair{std::string_view("-iquote"), &dirs.quote},
                    std::pair{std::string_view("-isystem"), &dirs.system},
                    std::pair{std::string_view("-I"), &dirs.user}}) {
                if (!a.starts_with(flag))
                    continue;
                list = target;
                value = a.substr(flag.size());
                if (value.empty() && i + 1 < args.size())
                    value = args[++i];
                break;
            }
            if (list == nullptr || value.empty())
                continue;
            const auto dir = normal(std::filesystem::path(build) / value);
            if (separate_build && is_under(dir, build))
                list->push_back(intern(dir, false));
            else if (is_under(dir, root_))
                list->push_back(intern(std::string(relative_to(dir, root_)),
                    true));
            else
                list->push_back(intern(dir, false));
        }

        const auto file = normal(std::filesystem::path(build) / c.file);
        std::string anchor;
        if (separate_build && is_under(file, build)) {
            const auto relative = relative_to(file, build);
            const auto cmake = relative.find("/CMakeFiles/");
            anchor = cmake == std::string_view::npos ? parent_of(relative)
                                                     : relative.substr(0, cmake);
        } else if (is_under(file, root_)) {
            anchor = parent_of(relative_to(file, root_));
        }

        const auto id = static_cast<std::uint32_t>(commands_.size());
        commands_.push_back(std::move(dirs));
        std::string_view up = anchor;
        for (;;) {
            by_directory_[std::string(up)].push_back(id);
            if (up.empty())
                break;
            up = parent_of(up);
        }
    }

    types::search_paths finish(const types::file_table& table) {
        by_directory_.try_emplace("");
        std::unordered_map<std::string, std::uint32_t> file_directory_contexts;
        out_.file_contexts.reserve(table.size());
        for (const auto& path : table.paths) {
            const auto directory = std::string(parent_of(path));
            auto it = file_directory_contexts.find(directory);
            if (it == file_directory_contexts.end()) {
                std::string_view up = directory;
                while (!by_directory_.contains(std::string(up)))
                    up = parent_of(up);
                it = file_directory_contexts
                         .emplace(directory, context_of(std::string(up)))
                         .first;
            }
            out_.file_contexts.push_back(it->second);
        }
        return std::move(out_);
    }

private:
    std::uint32_t intern(std::string path, bool internal) {
        auto key = (internal ? "i:" : "e:") + path;
        const auto [it, inserted] = directory_ids_.try_emplace(std::move(key),
            static_cast<std::uint32_t>(out_.directories.size()));
        if (inserted)
            out_.directories.push_back({std::move(path), internal});
        return it->second;
    }

    std::uint32_t context_of(const std::string& anchor) {
        const auto [it, inserted] = anchor_contexts_.try_emplace(anchor,
            static_cast<std::uint32_t>(out_.contexts.size()));
        if (!inserted)
            return it->second;

        const auto& ids = by_directory_.at(anchor);
        types::search_context context;
        const auto append_unique = [&](const std::vector<std::uint32_t>& from,
                                       std::size_t segment_start) {
            for (const auto d : from) {
                const auto segment = std::span(context.directories)
                                         .subspan(segment_start);
                if (std::ranges::find(segment, d) == segment.end())
                    context.directories.push_back(d);
            }
        };
        for (const auto id : ids)
            append_unique(commands_[id].quote, 0);
        context.first_angle = context.directories.size();
        for (const auto id : ids)
            append_unique(commands_[id].user, context.first_angle);
        const auto first_system = context.directories.size();
        for (const auto id : ids)
            append_unique(commands_[id].system, first_system);
        append_unique(implicit_, first_system);
        out_.contexts.push_back(std::move(context));
        return it->second;
    }

    std::string root_;
    std::vector<std::uint32_t> implicit_;
    std::vector<command_directories> commands_;
    std::unordered_map<std::string, std::vector<std::uint32_t>> by_directory_;
    std::unordered_map<std::string, std::uint32_t> directory_ids_;
    std::unordered_map<std::string, std::uint32_t> anchor_contexts_;
    types::search_paths out_;
};

}

types::search_paths build_search_paths(
    std::span<const types::compile_command> commands,
    const std::filesystem::path& source_root,
    std::span<const std::string> implicit_system,
    const types::file_table& table) {
    builder b(source_root, implicit_system);
    for (const auto& c : commands)
        b.add(c);
    return b.finish(table);
}

}
