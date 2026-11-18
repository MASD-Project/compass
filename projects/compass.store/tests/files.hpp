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
#ifndef COMPASS_STORE_TESTS_FILES_HPP
#define COMPASS_STORE_TESTS_FILES_HPP

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace compass::store::tests {

/**
 * @brief Writes @p content to @p path, creating its parent directories.
 */
inline void write(const std::filesystem::path& path, std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream os(path, std::ios::binary);
    os.write(content.data(), static_cast<std::streamsize>(content.size()));
}

inline std::string read(const std::filesystem::path& file) {
    std::ifstream is(file, std::ios::binary);
    std::ostringstream os;
    os << is.rdbuf();
    return os.str();
}

}

#endif
