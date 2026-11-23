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
 * Template: masd_cpp_codec_text_form.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_DB_CODEC_TEXT_FORM_HPP
#define COMPASS_DB_CODEC_TEXT_FORM_HPP

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace compass::db::codec {

/**
 * @brief The text a type takes on the wire.
 *
 * The types with a custom encoding whose wire form is a string state the
 * spelling of that string once here, so the JSON carrier writes the text it
 * reads, and a value that goes out comes back.
 *
 * A type that has a text form specialises this with three members: =wire_name=,
 * which names it in a refusal, =to_text=, and =from_text=. An enumeration's form
 * is generated beside the row that declares it, and also states =names=, the
 * text of each enumerator in declaration order, which its schema reads, and
 * =values=, the enumerators in the same order, which its samples read.
 *
 * The concrete forms are declared here and defined in text_form.cpp.
 */
template <typename T>
struct text_form;

/**
 * @brief A string is its own text form.
 */
template <>
struct text_form<std::string> {
    static constexpr std::string_view wire_name = "a string";

    static std::string to_text(const std::string& v);
    static std::string from_text(std::string_view text);
};

/**
 * @brief A filesystem path is its native string.
 */
template <>
struct text_form<std::filesystem::path> {
    static constexpr std::string_view wire_name = "a path";

    static std::string to_text(const std::filesystem::path& v);
    static std::filesystem::path from_text(std::string_view text);
};

/**
 * @brief An instant's text form is the wire shape the time architecture
 * states, six digits of microseconds and a UTC designator. Its year has four
 * digits, so an instant outside the years 0000 to 9999 has no text form.
 */
template <>
struct text_form<std::chrono::sys_time<std::chrono::microseconds>> {
    static constexpr std::string_view wire_name = "an instant";

    static constexpr std::pair<std::string_view, std::string_view> schema_keywords[] = {
        {"format", "date-time"},
        {"pattern", "^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(\\.[0-9]{1,6})?Z$"}};

    static std::string to_text(const std::chrono::sys_time<std::chrono::microseconds>& v);
    static std::chrono::sys_time<std::chrono::microseconds> from_text(std::string_view text);
};

}

#endif
