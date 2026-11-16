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
#ifndef COMPASS_CODEC_TYPES_CONFIGURATION_HPP
#define COMPASS_CODEC_TYPES_CONFIGURATION_HPP

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace compass::codec::types {

/**
 * @brief The key every codec has: "true" runs the codec and "false" does
 * not.
 */
inline constexpr std::string_view enabled_key = "enabled";

/**
 * @brief What the user set, one row per setting: the codec, the key and
 * the value. When two rows set the same key of a codec, the later one
 * wins.
 */
struct configuration {
    std::vector<std::string> codecs;
    std::vector<std::string> keys;
    std::vector<std::string> values;

    [[nodiscard]] std::size_t size() const noexcept { return codecs.size(); }

    void set(std::string_view codec, std::string_view key,
        std::string_view value) {
        codecs.emplace_back(codec);
        keys.emplace_back(key);
        values.emplace_back(value);
    }
};

}

#endif
