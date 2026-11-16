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
#ifndef COMPASS_ORG_TYPES_HANDLE_HPP
#define COMPASS_ORG_TYPES_HANDLE_HPP

#include <compare>
#include <cstdint>

namespace compass::org::types {

/**
 * @brief Dense 32-bit index of one row in one table.
 *
 * The tag names the table, so a handle of one table does not convert to a
 * handle of another and cannot index its columns. A handle is local to its
 * document: it counts rows from the first row the document has in that table.
 * The default value is none, which indexes no row.
 */
template <typename Tag>
struct handle {
    static constexpr std::uint32_t none_value = 0xFFFF'FFFF;

    std::uint32_t value = none_value;

    [[nodiscard]] static constexpr handle none() noexcept { return {}; }

    [[nodiscard]] constexpr bool is_none() const noexcept {
        return value == none_value;
    }

    [[nodiscard]] constexpr handle next() const noexcept {
        return {value + 1};
    }

    friend constexpr auto operator<=>(handle, handle) noexcept = default;
};

using document_id = handle<struct document_tag>;
using element_id = handle<struct element_tag>;
using object_id = handle<struct object_tag>;
using heading_id = handle<struct heading_tag>;
using tag_id = handle<struct heading_tag_tag>;
using keyword_id = handle<struct keyword_tag>;
using property_id = handle<struct property_tag>;
using affiliated_id = handle<struct affiliated_tag>;
using block_id = handle<struct block_tag>;
using list_id = handle<struct list_tag>;
using item_id = handle<struct item_tag>;
using link_id = handle<struct link_tag>;
using name_id = handle<struct name_tag>;

}

#endif
