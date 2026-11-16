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
#include "compass.store/ops/action_object.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include "compass.store/ops/blob_hash.hpp"

namespace compass::store::ops {

namespace {

constexpr std::string_view magic = "CMPSACT\n";
constexpr std::size_t digest_size = 20;

std::array<std::uint8_t, digest_size> checksum(const types::blob_id& key,
    std::string_view framed) {
    std::string bytes(reinterpret_cast<const char*>(key.bytes.data()),
        key.bytes.size());
    bytes += framed;
    return sha1_of(bytes);
}

}

std::string encode_action(const types::blob_id& key, std::string_view body) {
    std::string out(magic);
    out += body;
    const auto digest = checksum(key, out);
    out.append(reinterpret_cast<const char*>(digest.data()), digest.size());
    return out;
}

std::optional<std::string_view> decode_action(const types::blob_id& key,
    std::string_view bytes) {
    if (bytes.size() < magic.size() + digest_size ||
        !bytes.starts_with(magic))
        return std::nullopt;
    const auto framed = bytes.substr(0, bytes.size() - digest_size);
    const auto digest = checksum(key, framed);
    if (!std::ranges::equal(digest, bytes.substr(framed.size()), {}, {},
            [](char c) { return static_cast<std::uint8_t>(c); }))
        return std::nullopt;
    return framed.substr(magic.size());
}

}
