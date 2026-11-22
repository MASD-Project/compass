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
#include "compass.codegen/ops/library.hpp"

#include <algorithm>
#include <deque>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <system_error>
#include "compass.store/ops/blob_hash.hpp"

namespace compass::codegen::ops {

namespace {

namespace fs = std::filesystem;

std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
        s.remove_suffix(1);
    return s;
}

std::optional<std::string> read_disk(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
}

/**
 * @brief The regular files below @p directory, in generic form, sorted;
 * nothing when it is not a directory.
 */
std::vector<std::string> files_below(const fs::path& directory) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::is_directory(directory, ec))
        return out;
    for (auto it = fs::recursive_directory_iterator(directory, ec);
         !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (it->is_regular_file(ec))
            out.push_back(it->path().generic_string());
    }
    std::ranges::sort(out);
    return out;
}

/**
 * @brief The paths with_files_outside listed, which the views it returns
 * point into for as long as the reader lives.
 */
struct listed {
    std::mutex mutex;
    std::deque<std::string> paths;
};

}

fs::path compass_library_directory(const fs::path& executable) {
    return executable.parent_path().parent_path() / "share" / "compass" /
        "masd";
}

std::vector<std::string> library_directories(std::string_view option,
    const fs::path& executable) {
    std::vector<std::string> out;
    if (trim(option).empty())
        throw std::runtime_error("codegen: set the option library");
    while (true) {
        const auto comma = option.find(',');
        const auto entry = trim(option.substr(0, comma));
        if (entry.empty())
            throw std::runtime_error("codegen: the option library names an "
                "empty directory");
        std::string directory;
        if (entry == compass_library) {
            if (executable.empty())
                throw std::runtime_error("codegen: the option library names "
                    "@compass, but where this compass runs from is unknown");
            directory = compass_library_directory(executable)
                .lexically_normal().generic_string();
        } else {
            directory = fs::path(entry).generic_string();
        }
        while (directory.size() > 1 && directory.ends_with('/'))
            directory.pop_back();
        out.push_back(std::move(directory));
        if (comma == std::string_view::npos)
            break;
        option.remove_prefix(comma + 1);
    }
    return out;
}

bool outside_the_checkout(std::string_view directory) {
    return fs::path(directory).is_absolute();
}

checkout_reader with_files_outside(checkout_reader checkout) {
    auto state = std::make_shared<listed>();
    auto r = checkout;
    r.list = [list = checkout.list, state](std::string_view prefix,
                 std::string_view suffix) -> std::vector<std::string_view> {
        if (!outside_the_checkout(prefix))
            return list(prefix, suffix);
        const auto slash = prefix.rfind('/');
        const auto directory = slash == std::string_view::npos
            ? prefix : prefix.substr(0, slash);
        std::vector<std::string_view> out;
        const std::lock_guard lock(state->mutex);
        for (auto& path : files_below(fs::path(directory))) {
            if (!path.starts_with(prefix) || !path.ends_with(suffix))
                continue;
            out.emplace_back(state->paths.emplace_back(std::move(path)));
        }
        return out;
    };
    r.read = [read = checkout.read](std::string_view path) {
        return outside_the_checkout(path) ? read_disk(fs::path(path))
            : read(path);
    };
    r.exists = [exists = checkout.exists](std::string_view path) {
        if (!outside_the_checkout(path))
            return exists(path);
        std::error_code ec;
        return fs::is_regular_file(fs::path(path), ec);
    };
    return r;
}

std::string outside_files_digest(const std::vector<std::string>& directories) {
    std::string out;
    for (const auto& directory : directories) {
        if (!outside_the_checkout(directory))
            continue;
        out += directory;
        out.push_back('\0');
        for (const auto& path : files_below(fs::path(directory))) {
            const auto bytes = read_disk(fs::path(path));
            out += path;
            out.push_back('\0');
            out += bytes ? compass::store::ops::to_hex(
                compass::store::ops::blob_id_of(*bytes)) : std::string();
            out.push_back('\n');
        }
    }
    return out;
}

}
