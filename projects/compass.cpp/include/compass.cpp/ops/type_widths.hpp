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
#ifndef COMPASS_CPP_OPS_TYPE_WIDTHS_HPP
#define COMPASS_CPP_OPS_TYPE_WIDTHS_HPP

#include <cstdint>
#include <string_view>
#include <vector>
#include "compass.cpp/ops/lexer.hpp"

/**
 * @brief How many data members each class, struct and union of a C++
 * source declares.
 */
namespace compass::cpp::ops {

/**
 * @brief A named class, struct or union a source defines, at any scope:
 * its key, its name, where its key is, and how many non-static data
 * members its body declares, one per declarator, so int a, b; is two.
 * Members of nested types count for the nested type alone. A template
 * specialisation is named without its arguments.
 */
struct type_width {
    std::string_view key;
    std::string_view name;
    std::uint32_t offset = 0;
    std::uint32_t fields = 0;
};

/**
 * @brief Each type @p l defines, in the order its key comes. Its views
 * point into l.code.
 */
[[nodiscard]] std::vector<type_width> type_widths(const lexed& l);

}

#endif
