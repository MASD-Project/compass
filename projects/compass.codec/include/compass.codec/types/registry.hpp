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
#ifndef COMPASS_CODEC_TYPES_REGISTRY_HPP
#define COMPASS_CODEC_TYPES_REGISTRY_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "compass.codec/types/descriptor.hpp"
#include "compass.org/types/handle.hpp"

namespace compass::codec::types {

using codec_id = compass::org::types::handle<struct codec_tag>;

/**
 * @brief The codecs of a run, one row per codec, indexed by codec_id, with
 * what the type registry and the configuration made of each descriptor.
 *
 * enabled is 1 for a codec that runs. The option values of codec c are the
 * entries of option_values from option_firsts[c], one per option its
 * descriptor declares and in that order: the configured value, or the
 * default. reads holds type_slots flags per codec, one per row of the type
 * registry and a last one for documents with no registered type; a flag is
 * 1 when the codec reads the documents of that type.
 *
 * problems says what did not fit: a codec name used twice, an input type
 * the type registry does not hold, a setting for a codec or a key nobody
 * declared, an enabled value that is neither true nor false. Each one
 * leaves the rest of the registry as it would be without it.
 */
struct registry {
    std::vector<descriptor> codecs;
    std::vector<std::uint8_t> enabled;
    std::vector<std::uint32_t> option_firsts;
    std::vector<std::string> option_values;
    std::size_t type_slots = 0;
    std::vector<std::uint8_t> reads;
    std::vector<std::string> problems;

    [[nodiscard]] std::size_t size() const noexcept { return codecs.size(); }
};

}

#endif
