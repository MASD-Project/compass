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
#ifndef COMPASS_PLATFORM_RANDOM_SECURE_RANDOM_HPP
#define COMPASS_PLATFORM_RANDOM_SECURE_RANDOM_HPP

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace compass::platform::random {

/**
 * @brief Fills @p out with bytes from the system's cryptographically secure
 * generator: getentropy on POSIX, BCryptGenRandom on Windows.
 *
 * @throws std::system_error when the system gives none.
 */
void fill_secure_random(std::span<std::byte> out);

/**
 * @brief @p length characters drawn uniformly from @p alphabet with the
 * secure generator, as a secret is made.
 *
 * @pre @p alphabet holds between 1 and 256 characters.
 * @throws std::system_error when the system gives no random bytes.
 */
[[nodiscard]] std::string secure_random_string(std::size_t length,
    std::string_view alphabet);

}

#endif
