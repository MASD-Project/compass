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
#include "compass.store/ops/blob_hash.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace compass::store::ops {

namespace {

/**
 * @brief SHA-1 as FIPS 180-4 defines it, fed in any number of pieces.
 */
class sha1 final {
public:
    void update(std::string_view data) noexcept {
        size_ += data.size();
        if (buffered_ != 0) {
            const auto take = std::min(data.size(), block_.size() - buffered_);
            std::memcpy(block_.data() + buffered_, data.data(), take);
            buffered_ += take;
            data.remove_prefix(take);
            if (buffered_ < block_.size())
                return;
            compress(block_.data());
            buffered_ = 0;
        }
        while (data.size() >= block_.size()) {
            compress(reinterpret_cast<const std::uint8_t*>(data.data()));
            data.remove_prefix(block_.size());
        }
        if (!data.empty())
            std::memcpy(block_.data(), data.data(), data.size());
        buffered_ = data.size();
    }

    [[nodiscard]] std::array<std::uint8_t, 20> finish() noexcept {
        const std::uint64_t bits = size_ * 8;
        block_[buffered_++] = 0x80;
        if (buffered_ > 56) {
            std::memset(block_.data() + buffered_, 0,
                block_.size() - buffered_);
            compress(block_.data());
            buffered_ = 0;
        }
        std::memset(block_.data() + buffered_, 0, 56 - buffered_);
        for (std::size_t i = 0; i < 8; ++i)
            block_[56 + i] = static_cast<std::uint8_t>(bits >> (56 - 8 * i));
        compress(block_.data());

        std::array<std::uint8_t, 20> digest;
        for (std::size_t i = 0; i < 5; ++i)
            for (std::size_t j = 0; j < 4; ++j)
                digest[4 * i + j] =
                    static_cast<std::uint8_t>(state_[i] >> (24 - 8 * j));
        return digest;
    }

private:
    void compress(const std::uint8_t* block) noexcept {
        std::array<std::uint32_t, 80> w;
        for (std::size_t i = 0; i < 16; ++i)
            w[i] = std::uint32_t{block[4 * i]} << 24 |
                std::uint32_t{block[4 * i + 1]} << 16 |
                std::uint32_t{block[4 * i + 2]} << 8 |
                std::uint32_t{block[4 * i + 3]};
        for (std::size_t i = 16; i < 80; ++i)
            w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        auto e = state_[4];
        for (std::size_t i = 0; i < 80; ++i) {
            std::uint32_t f;
            std::uint32_t k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            const auto t = std::rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = t;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
    }

    std::array<std::uint32_t, 5> state_{0x67452301, 0xEFCDAB89, 0x98BADCFE,
        0x10325476, 0xC3D2E1F0};
    std::array<std::uint8_t, 64> block_{};
    std::size_t buffered_ = 0;
    std::uint64_t size_ = 0;
};

}

types::blob_id blob_id_of(std::string_view content) noexcept {
    std::array<char, 32> header{'b', 'l', 'o', 'b', ' '};
    const auto end = std::to_chars(header.data() + 5,
        header.data() + header.size() - 1, content.size()).ptr;
    *end = '\0';

    sha1 h;
    h.update({header.data(), static_cast<std::size_t>(end - header.data()) +
        1});
    h.update(content);
    return {h.finish()};
}

std::string to_hex(const types::blob_id& id) {
    constexpr std::string_view digits = "0123456789abcdef";
    std::string hex;
    hex.reserve(40);
    for (const auto byte : id.bytes) {
        hex += digits[byte >> 4];
        hex += digits[byte & 0xF];
    }
    return hex;
}

}
