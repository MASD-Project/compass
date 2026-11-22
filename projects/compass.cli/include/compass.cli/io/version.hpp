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
#ifndef COMPASS_CLI_IO_VERSION_HPP
#define COMPASS_CLI_IO_VERSION_HPP

#include <filesystem>
#include <string>
#include <string_view>

namespace compass::cli::io {

/**
 * @brief The name of the build manifest the build writes beside the
 * compass binary: key=value lines naming the commit, its time, whether the
 * tree was dirty and the branch, and, once installed, when.
 */
inline constexpr std::string_view manifest_name = "compass.manifest";

/**
 * @brief The version of the build whose binary is @p executable, one fact
 * per line: the release; the release tag on the commit, or none, naming
 * the CI run of a pre-release build the install script installed; the full
 * and short commit; whether its tree was dirty; the branch; the commit's committer time; the build time, which is
 * the binary's last write time; when it was installed; and the store
 * formats it reads and writes.
 *
 * The commit facts come from the manifest beside @p executable. They are
 * "unknown" when @p executable is empty or the manifest is missing or
 * damaged; the release and the store formats are compiled in.
 */
[[nodiscard]] std::string version_text(
    const std::filesystem::path& executable);

}

#endif
