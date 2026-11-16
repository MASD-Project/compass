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
#ifndef COMPASS_CODEC_OPS_REGISTRY_HPP
#define COMPASS_CODEC_OPS_REGISTRY_HPP

#include <span>
#include <string>
#include "compass.codec/types/configuration.hpp"
#include "compass.codec/types/descriptor.hpp"
#include "compass.codec/types/registry.hpp"
#include "compass.org/types/type_registry.hpp"

namespace compass::codec::ops {

/**
 * @brief Builds the registry of @p codecs: resolves the input types of
 * each against @p types and applies @p settings to its options and to
 * whether it runs.
 *
 * Nothing here or in the runner knows a codec by name: a codec is a row,
 * and what it does differently is in its descriptor. A codec whose name an
 * earlier one has is left out.
 */
[[nodiscard]] types::registry make_registry(
    std::span<const types::descriptor> codecs,
    const compass::org::types::type_registry& types,
    const types::configuration& settings);

/**
 * @brief True when @p codec reads documents of type @p type; none asks
 * about documents with no registered type.
 */
[[nodiscard]] bool reads(const types::registry& r, types::codec_id codec,
    compass::org::types::type_id type) noexcept;

/**
 * @brief The option values of @p codec, in the order its descriptor
 * declares the options.
 */
[[nodiscard]] std::span<const std::string> option_values(
    const types::registry& r, types::codec_id codec) noexcept;

}

#endif
