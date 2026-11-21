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
#include "compass.platform/random/secure_random.hpp"

#include <algorithm>
#include <array>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#else
#include <cerrno>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/random.h>
#endif
#endif

namespace compass::platform::random {

void fill_secure_random(std::span<std::byte> out) {
#if defined(_WIN32)
    while (!out.empty()) {
        const auto n = static_cast<ULONG>(
            std::min<std::size_t>(out.size(), 1u << 20));
        const auto status = ::BCryptGenRandom(nullptr,
            reinterpret_cast<PUCHAR>(out.data()), n,
            BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status < 0)
            throw std::system_error(static_cast<int>(status),
                std::system_category(), "BCryptGenRandom");
        out = out.subspan(n);
    }
#else
    // getentropy gives at most 256 bytes a call.
    constexpr std::size_t most = 256;
    while (!out.empty()) {
        const auto n = std::min(out.size(), most);
        if (::getentropy(out.data(), n) != 0)
            throw std::system_error(errno, std::generic_category(),
                "getentropy");
        out = out.subspan(n);
    }
#endif
}

std::string secure_random_string(std::size_t length,
    std::string_view alphabet) {
    // A byte at or above the largest multiple of the alphabet's size is
    // drawn again, so every character is as likely as every other.
    const std::size_t size = alphabet.size();
    const std::size_t limit = 256 - 256 % size;
    std::string out;
    out.reserve(length);
    std::array<std::byte, 64> pool{};
    std::size_t at = pool.size();
    while (out.size() < length) {
        if (at == pool.size()) {
            fill_secure_random(pool);
            at = 0;
        }
        const auto b = static_cast<std::size_t>(pool[at++]);
        if (b < limit)
            out.push_back(alphabet[b % size]);
    }
    return out;
}

}
