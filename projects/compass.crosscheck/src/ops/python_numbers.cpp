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
#include "compass.crosscheck/ops/python_numbers.hpp"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include "compass.cpp/ops/text.hpp"

namespace compass::crosscheck::ops {

namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }

/**
 * @brief @p digits with the underscores Python allows between two digits
 * removed; nothing when an underscore stands anywhere else.
 */
std::optional<std::string> without_underscores(std::string_view digits) {
    std::string out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (digits[i] != '_') {
            out.push_back(digits[i]);
            continue;
        }
        if (i == 0 || i + 1 == digits.size() || !is_digit(digits[i - 1]) ||
            !is_digit(digits[i + 1]))
            return std::nullopt;
    }
    return out;
}

std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

}

std::optional<std::string> python_int(std::string_view text) {
    auto t = compass::cpp::ops::strip(text);
    bool negative = false;
    if (!t.empty() && (t.front() == '+' || t.front() == '-')) {
        negative = t.front() == '-';
        t.remove_prefix(1);
    }
    if (t.empty() || !is_digit(t.front()) || !is_digit(t.back()))
        return std::nullopt;
    auto digits = without_underscores(t);
    if (!digits)
        return std::nullopt;
    for (const auto c : *digits) {
        if (!is_digit(c))
            return std::nullopt;
    }
    const auto first = digits->find_first_not_of('0');
    if (first == std::string::npos)
        return std::string("0");
    return (negative ? "-" : "") + digits->substr(first);
}

std::string python_float_repr(double value) {
    if (std::isnan(value))
        return "nan";
    if (std::isinf(value))
        return value < 0 ? "-inf" : "inf";
    std::array<char, 64> buffer{};
    const auto [end, ec] = std::to_chars(buffer.data(),
        buffer.data() + buffer.size(), value, std::chars_format::scientific);
    std::string_view s(buffer.data(), static_cast<std::size_t>(end -
        buffer.data()));
    std::string sign;
    if (s.front() == '-') {
        sign = "-";
        s.remove_prefix(1);
    }
    const auto e = s.find('e');
    std::string digits;
    for (const auto c : s.substr(0, e)) {
        if (c != '.')
            digits.push_back(c);
    }
    int exponent = 0;
    std::from_chars(s.data() + e + 1 + (s[e + 1] == '+' ? 1 : 0),
        s.data() + s.size(), exponent);
    if (value == 0)
        return sign + "0.0";
    if (exponent >= -4 && exponent < 16) {
        const auto point = exponent + 1;
        std::string out;
        if (point <= 0) {
            out = "0." + std::string(static_cast<std::size_t>(-point), '0') +
                digits;
        } else if (static_cast<std::size_t>(point) >= digits.size()) {
            out = digits + std::string(static_cast<std::size_t>(point) -
                digits.size(), '0') + ".0";
        } else {
            out = digits.substr(0, static_cast<std::size_t>(point)) + "." +
                digits.substr(static_cast<std::size_t>(point));
        }
        return sign + out;
    }
    std::string mantissa = digits.substr(0, 1);
    if (digits.size() > 1)
        mantissa += "." + digits.substr(1);
    const auto magnitude = std::to_string(exponent < 0 ? -exponent : exponent);
    return sign + mantissa + "e" + (exponent < 0 ? "-" : "+") +
        (magnitude.size() < 2 ? "0" : "") + magnitude;
}

std::optional<std::string> python_float(std::string_view text) {
    auto t = compass::cpp::ops::strip(text);
    std::string_view body = t;
    if (!body.empty() && (body.front() == '+' || body.front() == '-'))
        body.remove_prefix(1);
    const auto word = lower(body);
    if (word == "inf" || word == "infinity" || word == "nan") {
        const bool negative = !t.empty() && t.front() == '-';
        if (word == "nan")
            return std::string("nan");
        return std::string(negative ? "-inf" : "inf");
    }
    const auto plain = without_underscores(t);
    if (!plain || plain->empty())
        return std::nullopt;
    for (const auto c : *plain) {
        if (!is_digit(c) && c != '.' && c != 'e' && c != 'E' && c != '+' &&
            c != '-')
            return std::nullopt;
    }
    char* stop = nullptr;
    const double value = std::strtod(plain->c_str(), &stop);
    if (stop != plain->c_str() + plain->size())
        return std::nullopt;
    return python_float_repr(value);
}

}
