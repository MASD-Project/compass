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
#ifndef COMPASS_PLATFORM_COMPILER_ALWAYS_INLINE_HPP
#define COMPASS_PLATFORM_COMPILER_ALWAYS_INLINE_HPP

/**
 * @brief Asks the compiler to inline a lambda, placed between its capture and
 * its parameter list, where C++23 applies an attribute to the call operator.
 * GCC rejects a GNU attribute between the parameter list and noexcept, and
 * clang one after noexcept. MSVC has no such attribute for a lambda, so there
 * it asks nothing and the optimiser decides.
 */
#if defined(_MSC_VER) && !defined(__clang__)
#define COMPASS_ALWAYS_INLINE
#else
#define COMPASS_ALWAYS_INLINE [[gnu::always_inline]]
#endif

#endif
