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
#ifndef COMPASS_CODEC_TYPES_FACT_HPP
#define COMPASS_CODEC_TYPES_FACT_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace compass::codec::types {

/**
 * @brief Appends the fields of a scan's fact to a string: a number as four
 * little-endian bytes, a text as its length as a number then its bytes.
 */
class fact_writer final {
public:
    explicit fact_writer(std::string& out) noexcept : out_(&out) {}

    fact_writer& number(std::uint32_t v) {
        for (int shift = 0; shift < 32; shift += 8)
            out_->push_back(static_cast<char>((v >> shift) & 0xFF));
        return *this;
    }

    fact_writer& text(std::string_view t) {
        number(static_cast<std::uint32_t>(t.size()));
        out_->append(t);
        return *this;
    }

private:
    std::string* out_;
};

/**
 * @brief Reads the fields fact_writer wrote, in the order it wrote them. A
 * read past the end gives zero or an empty text.
 */
class fact_reader final {
public:
    explicit fact_reader(std::string_view fact) noexcept : fact_(fact) {}

    [[nodiscard]] bool done() const noexcept { return fact_.empty(); }

    std::uint32_t number() noexcept {
        if (fact_.size() < 4) {
            fact_ = {};
            return 0;
        }
        std::uint32_t v = 0;
        for (int i = 3; i >= 0; --i)
            v = (v << 8) | static_cast<std::uint8_t>(fact_[i]);
        fact_.remove_prefix(4);
        return v;
    }

    std::string_view text() noexcept {
        const auto n = number();
        const auto r = fact_.substr(0, n);
        fact_.remove_prefix(r.size());
        return r;
    }

private:
    std::string_view fact_;
};

}

#endif
