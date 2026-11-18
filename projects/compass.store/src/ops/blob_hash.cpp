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
#include "compass.platform/compiler/always_inline.hpp"

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
    /**
     * @brief One block. The rounds are unrolled, five to a line, and the
     * message schedule is a ring of sixteen words filled as the rounds
     * need it, which hashes about two and a half times faster than a loop
     * over an eighty-word schedule. Hashing is half the time of an edit
     * hook, which checks every record and cache object it reads.
     */
    void compress(const std::uint8_t* block) noexcept {
        std::uint32_t w[16];
        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        auto e = state_[4];
        const auto load = [&](int i) COMPASS_ALWAYS_INLINE noexcept {
            std::uint32_t v;
            std::memcpy(&v, block + 4 * i, 4);
            if constexpr (std::endian::native == std::endian::little)
                v = std::byteswap(v);
            return w[i] = v;
        };
        const auto mix = [&](int i) COMPASS_ALWAYS_INLINE noexcept {
            return w[i & 15] = std::rotl(w[(i + 13) & 15] ^ w[(i + 8) & 15] ^
                w[(i + 2) & 15] ^ w[i & 15], 1);
        };
        const auto choose = [](std::uint32_t x, std::uint32_t y,
            std::uint32_t z) noexcept { return z ^ (x & (y ^ z)); };
        const auto parity = [](std::uint32_t x, std::uint32_t y,
            std::uint32_t z) noexcept { return x ^ y ^ z; };
        const auto majority = [](std::uint32_t x, std::uint32_t y,
            std::uint32_t z) noexcept { return (x & y) | (z & (x | y)); };
        const auto round = [](std::uint32_t f, std::uint32_t k,
            std::uint32_t word, std::uint32_t v, std::uint32_t& x,
            std::uint32_t& z) COMPASS_ALWAYS_INLINE noexcept {
            z += std::rotl(v, 5) + f + k + word;
            x = std::rotl(x, 30);
        };
        const auto five = [&](auto&& word, auto&& f, std::uint32_t k,
            int i) COMPASS_ALWAYS_INLINE noexcept {
            round(f(b, c, d), k, word(i), a, b, e);
            round(f(a, b, c), k, word(i + 1), e, a, d);
            round(f(e, a, b), k, word(i + 2), d, e, c);
            round(f(d, e, a), k, word(i + 3), c, d, b);
            round(f(c, d, e), k, word(i + 4), b, c, a);
        };
        constexpr std::uint32_t k1 = 0x5A827999;
        constexpr std::uint32_t k2 = 0x6ED9EBA1;
        constexpr std::uint32_t k3 = 0x8F1BBCDC;
        constexpr std::uint32_t k4 = 0xCA62C1D6;
        five(load, choose, k1, 0);
        five(load, choose, k1, 5);
        five(load, choose, k1, 10);
        round(choose(b, c, d), k1, load(15), a, b, e);
        round(choose(a, b, c), k1, mix(16), e, a, d);
        round(choose(e, a, b), k1, mix(17), d, e, c);
        round(choose(d, e, a), k1, mix(18), c, d, b);
        round(choose(c, d, e), k1, mix(19), b, c, a);
        for (int i = 20; i < 40; i += 5)
            five(mix, parity, k2, i);
        for (int i = 40; i < 60; i += 5)
            five(mix, majority, k3, i);
        for (int i = 60; i < 80; i += 5)
            five(mix, parity, k4, i);
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

std::array<std::uint8_t, 20> sha1_of(std::string_view data) noexcept {
    sha1 h;
    h.update(data);
    return h.finish();
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
