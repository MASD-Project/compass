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
#ifndef COMPASS_DB_CODEC_BLOB_ID_FORM_HPP
#define COMPASS_DB_CODEC_BLOB_ID_FORM_HPP

#include <stdexcept>
#include <string>
#include <string_view>
#include "compass.db/codec/text_form.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "compass.store/ops/git_object.hpp"
#include "compass.store/types/blob_id.hpp"

namespace compass::db::codec {

/**
 * @brief A blob ID's text form is the ID as git prints it: 40 lower-case
 * hexadecimal digits.
 */
template <>
struct text_form<store::types::blob_id> {
    static constexpr std::string_view wire_name = "a blob ID";

    static constexpr std::string_view sample_texts[] = {
        "e69de29bb2d1d6434b8b29ae775ad8c2e48c5391",
        "8ab686eafeb1f44702738c8b0f24f2567c36da6d"};

    static std::string to_text(const store::types::blob_id& v) {
        return store::ops::to_hex(v);
    }

    static store::types::blob_id from_text(std::string_view text) {
        if (const auto id = store::ops::from_hex(text))
            return *id;
        throw std::invalid_argument("Not " + std::string(wire_name) + ": " +
            std::string(text) + ": not 40 hexadecimal digits");
    }
};

}

#endif
