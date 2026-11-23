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
/**
 * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY
 * Template: masd_cpp_codec_text_form_impl.cpp.mustache
 * To modify, update the template and regenerate.
 */
#include "compass.db/codec/text_form.hpp"

#include <chrono>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>

namespace compass::db::codec {

namespace {

[[noreturn]] void refuse(std::string_view what, std::string_view text, std::string_view why) {
    throw std::invalid_argument("Not " + std::string(what) + ": " + std::string(text) + ": " +
                                std::string(why));
}

/**
 * @brief The number the @p width digits at @p at spell, or none when one of
 * them is not a digit.
 */
int digits(std::string_view text, std::size_t at, std::size_t width) {
    int out = 0;
    for (std::size_t i = at; i < at + width; ++i) {
        if (text[i] < '0' || text[i] > '9')
            return -1;
        out = out * 10 + (text[i] - '0');
    }
    return out;
}

}

std::string text_form<std::string>::to_text(const std::string& v) {
    return v;
}

std::string text_form<std::string>::from_text(std::string_view text) {
    return std::string(text);
}

std::string text_form<std::filesystem::path>::to_text(const std::filesystem::path& v) {
    return v.generic_string();
}

std::filesystem::path text_form<std::filesystem::path>::from_text(std::string_view text) {
    return std::filesystem::path(std::string(text));
}

std::string text_form<std::chrono::sys_time<std::chrono::microseconds>>::to_text(
    const std::chrono::sys_time<std::chrono::microseconds>& v) {
    return std::format("{:%FT%TZ}", v);
}

std::chrono::sys_time<std::chrono::microseconds>
text_form<std::chrono::sys_time<std::chrono::microseconds>>::from_text(std::string_view text) {
    using namespace std::chrono;
    // YYYY-MM-DDTHH:MM:SS, then a fraction of one to six digits, then Z.
    if (text.size() < 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text.back() != 'Z')
        refuse(wire_name, text, "expected YYYY-MM-DDTHH:MM:SS.ffffffZ");
    const auto y = digits(text, 0, 4);
    const auto mo = digits(text, 5, 2);
    const auto d = digits(text, 8, 2);
    const auto h = digits(text, 11, 2);
    const auto mi = digits(text, 14, 2);
    const auto s = digits(text, 17, 2);
    if (y < 0 || mo < 0 || d < 0 || h < 0 || mi < 0 || s < 0 || h > 23 || mi > 59 || s > 59)
        refuse(wire_name, text, "a field is not a number in range");
    long long fraction = 0;
    const auto rest = text.substr(19, text.size() - 20);
    if (!rest.empty()) {
        if (rest.front() != '.' || rest.size() < 2 || rest.size() > 7)
            refuse(wire_name, text, "the fraction is one to six digits after a point");
        const auto f = digits(rest, 1, rest.size() - 1);
        if (f < 0)
            refuse(wire_name, text, "the fraction is not a number");
        fraction = f;
        for (auto n = rest.size() - 1; n < 6; ++n)
            fraction *= 10;
    }
    const year_month_day date{year{y}, month{static_cast<unsigned>(mo)},
                              day{static_cast<unsigned>(d)}};
    if (!date.ok())
        refuse(wire_name, text, "not a calendar date");
    return sys_days{date} + hours{h} + minutes{mi} + seconds{s} + microseconds{fraction};
}

}
