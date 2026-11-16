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
#ifndef COMPASS_BENCH_TYPES_FILE_TABLE_HPP
#define COMPASS_BENCH_TYPES_FILE_TABLE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace compass::bench::types {

/**
 * @brief Hash that lets the extension index be probed with a string_view.
 */
struct string_hash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

/**
 * @brief The files a stage works on, as parallel columns indexed by file id.
 *
 * Paths are relative to the checkout root. Extensions are interned once:
 * extension_ids holds an index into extensions, and the empty string stands
 * for a file with no extension.
 */
struct file_table {
    std::vector<std::string> paths;
    std::vector<std::uint64_t> sizes;
    std::vector<std::uint32_t> extension_ids;
    std::vector<std::string> extensions;
    std::unordered_map<std::string, std::uint32_t, string_hash,
        std::equal_to<>> extension_index;

    [[nodiscard]] std::size_t size() const noexcept { return paths.size(); }
};

/**
 * @brief A half-open run of file ids [begin, end) handed to one thread.
 */
struct file_range {
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
};

}

#endif
