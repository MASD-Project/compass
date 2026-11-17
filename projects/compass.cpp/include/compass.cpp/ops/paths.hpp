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
#ifndef COMPASS_CPP_OPS_PATHS_HPP
#define COMPASS_CPP_OPS_PATHS_HPP

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace compass::cpp::ops {

/**
 * @brief The suffix pathlib gives the last component of @p path: from its
 * last dot, once the dots it starts with are set aside; empty when there
 * is no other dot.
 */
[[nodiscard]] std::string_view suffix_of(std::string_view path) noexcept;

/**
 * @brief The last component of @p path without its suffix_of.
 */
[[nodiscard]] std::string_view stem_of(std::string_view path) noexcept;

/**
 * @brief The directory part of @p path: everything before its last '/',
 * empty for a path with none.
 */
[[nodiscard]] std::string_view parent_of(std::string_view path) noexcept;

/**
 * @brief The components of @p path between its slashes, in order.
 */
[[nodiscard]] std::vector<std::string_view> components_of(
    std::string_view path);

/**
 * @brief True when @p a sorts before @p b as pathlib sorts paths: by their
 * components, in turn.
 */
[[nodiscard]] bool component_less(std::string_view a,
    std::string_view b) noexcept;

/**
 * @brief True when @p path matches @p pattern as pathlib's glob matches a
 * file: component by component, "*" any run of characters but "/", "?"
 * one character, and a "**" component zero or more directories.
 */
[[nodiscard]] bool glob_matches(std::string_view pattern,
    std::string_view path);

/**
 * @brief The files of a checkout, paths relative to its root in byte
 * order, and the directories they imply.
 */
class file_set final {
public:
    explicit file_set(std::span<const std::string_view> sorted) noexcept
        : paths_(sorted) {}

    [[nodiscard]] bool is_file(std::string_view path) const noexcept;

    /**
     * @brief True when some file is under @p path; the empty path, the
     * root of the checkout, is a directory.
     */
    [[nodiscard]] bool is_dir(std::string_view path) const;

    /**
     * @brief The file @p name names from directory @p dir, as the
     * operating system finds it: "." and empty components are dropped and
     * ".." goes up from a directory that exists. Nothing when the path is
     * absolute, leaves the checkout, passes through a directory that does
     * not exist or names no file.
     */
    [[nodiscard]] std::optional<std::string> find(std::string_view dir,
        std::string_view name) const;

private:
    std::span<const std::string_view> paths_;
};

}

#endif
