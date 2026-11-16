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
#ifndef COMPASS_MODEL_OPS_LOADERS_HPP
#define COMPASS_MODEL_OPS_LOADERS_HPP

#include <string>
#include <string_view>
#include "compass.model/ops/builder.hpp"
#include "compass.model/ops/load.hpp"
#include "compass.model/ops/outline.hpp"

/*
 * One loader per org_loader function of ores.codegen. Each fills the root
 * object of one model.
 */
namespace compass::model::ops {

struct load_context {
    builder& b;
    const document& d;
    const outline& o;
    const document_finder& find;
};

/**
 * @brief {kind UUID: [code, ...]} of every source block with :implements,
 * in document order, as _collect_implementations builds it; nothing is
 * written when there is none.
 */
void collect_implementations(load_context& c, types::value_id parent,
    std::string_view key);

/**
 * @brief The value of frontmatter @p key, or @p fallback.
 */
[[nodiscard]] std::string_view frontmatter_or(const outline& o,
    std::string_view key, std::string_view fallback = {});

/**
 * @brief The file name of the document: the last component of its path.
 */
[[nodiscard]] std::string_view file_name(const document& d) noexcept;

void load_component(load_context& c, types::value_id root);
void load_component_overview(load_context& c, types::value_id root);

}

#endif
