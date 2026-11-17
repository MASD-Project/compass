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
#ifndef COMPASS_CPP_OPS_DECLARATIONS_HPP
#define COMPASS_CPP_OPS_DECLARATIONS_HPP

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include "compass.cpp/ops/source_model.hpp"

/**
 * @brief The namespace-scope declarations the one-definition gates read.
 */
namespace compass::cpp::ops {

/**
 * @brief Each qualified class, struct, union or scoped enum that @p m
 * defines directly in named namespaces, with the offset of the name of
 * its first definition.
 */
[[nodiscard]] std::map<std::string, std::uint32_t> defined_types(
    const source_model& m);

/**
 * @brief Each function or variable of @p m with internal linkage, by its
 * enclosing named namespaces and its name, with the offset where its
 * first declaration starts. A function's name carries the types of its
 * parameters, as in f(conststd::string&,int), since overloads that differ
 * in them do not collide. Internal is: inside an anonymous namespace,
 * static, or const or constexpr and not inline, as the specifiers before
 * the type spell it. An extern declaration and a definition of a
 * qualified name are not counted.
 */
[[nodiscard]] std::map<std::pair<std::string, std::string>, std::uint32_t>
internal_names(const source_model& m);

}

#endif
