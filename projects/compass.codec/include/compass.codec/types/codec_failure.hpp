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
#ifndef COMPASS_CODEC_TYPES_CODEC_FAILURE_HPP
#define COMPASS_CODEC_TYPES_CODEC_FAILURE_HPP

#include <stdexcept>
#include <string>
#include <utility>

namespace compass::codec::types {

/**
 * @brief What a codec threw while a run called it, with the codec's name.
 * what() is the codec's own message, so a host that ignores the codec
 * reports the failure as the codec wrote it.
 */
class codec_failure final : public std::runtime_error {
public:
    codec_failure(std::string codec, const std::string& what)
        : std::runtime_error(what), codec_(std::move(codec)) {}

    [[nodiscard]] const std::string& codec() const noexcept { return codec_; }

private:
    std::string codec_;
};

}

#endif
