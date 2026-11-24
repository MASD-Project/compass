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
 * Template: masd_cpp_codec_sample.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_DB_CODEC_SAMPLE_HPP
#define COMPASS_DB_CODEC_SAMPLE_HPP

#include "compass.db/codec/text_form.hpp"
#include "compass.db/codec/wire_fields.hpp"
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

/**
 * @brief Non-default values for every type the wire codec writes.
 *
 * A roundtrip test fills a value with these, writes it, reads it back and
 * requires the same value. Every member must differ from its default, or a
 * codec that drops the member would still pass. The dispatch covers the
 * codec's set: a struct that states its wire_fields, a type with a text form,
 * the scalars, and the optional, vector and map wrappers. A type
 * outside that set fails to compile here, which names the gap.
 *
 * Each struct has two samples. In the first, every member differs from its
 * default. The second differs from the first in every member, and a member
 * whose modelled default equals the first takes it. A signed integer's second
 * sample is negative. Every value is held exactly by both formats: no NaN and
 * no computed double.
 *
 * A sample is numbered, and member j of a struct takes the numbers 2j and
 * 2j + 1, so two members of one type hold different values. Otherwise a codec
 * that reads one member's key into another of the same type would pass. A type
 * with fewer values than members repeats them: bool, an enumeration and a type
 * with only its two sample texts.
 *
 * A type that states its own text_form outside this header states its two
 * samples there too, as sample_texts, and the sampler reads them back through
 * from_text. The type's owner knows which texts are valid.
 *
 * An enumeration takes its samples from the values its generated text_form
 * states, not through from_text, so a from_text that misreads a name fails
 * the round trip.
 */
namespace compass::db::codec {

template <typename T>
T sample(std::size_t which);

template <std::equality_comparable T>
T sample_unlike(const T& avoid, std::size_t which = 0) {
    auto first = sample<T>(which);
    if (first == avoid)
        return sample<T>(which + 1);
    return first;
}

namespace detail {

template <typename T>
inline constexpr bool unsampled = false;

// signed char and unsigned char are std::int8_t and std::uint8_t, which the
// codec writes as numbers, so only the plain character types are refused.
template <typename T>
inline constexpr bool is_character =
    std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char8_t> ||
    std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

template <typename T>
struct sampler {
    static T make(std::size_t which) {
        if constexpr (states_wire_form<T>) {
            constexpr std::size_t count = decltype(wire_fields(std::type_identity<T>{}))::size;
            const std::size_t base = 2 * count * (which / 2);
            const auto unlike = [base](const T& avoid) {
                T out{};
                apply_fields(
                    [&](const auto&... fields) {
                        std::size_t j = 0;
                        ((out.*(fields.member) =
                              sample_unlike(avoid.*(fields.member), base + 2 * j++)),
                         ...);
                    },
                    wire_fields(std::type_identity<T>{}));
                return out;
            };
            // The second sample differs from the first in every member, so two
            // elements of a vector of structs are never equal.
            const auto first = unlike(T{});
            return which % 2 == 0 ? first : unlike(first);
        } else if constexpr (requires { text_form<T>::values; }) {
            constexpr auto& values = text_form<T>::values;
            static_assert(std::size(values) > 0, "an enumeration with no enumerators");
            constexpr std::size_t first = std::size(values) > 1 ? 1 : 0;
            if (which < 2)
                return values[which == 0 ? first : 0];
            return values[which % std::size(values)];
        } else if constexpr (std::is_same_v<T, bool>) {
            return which % 2 == 0;
        } else if constexpr (std::is_integral_v<T> && !is_character<T>) {
            constexpr auto max = std::numeric_limits<T>::max();
            if (which == 0)
                return static_cast<T>(max - 1);
            // Kept inside the type's range, away from zero and from the first
            // sample, so a narrow integer never wraps to its default.
            if constexpr (std::is_signed_v<T>) {
                const auto step = static_cast<T>((which / 2) % (max / 2));
                return which % 2 == 0 ? static_cast<T>(max - 1 - step) :
                                        static_cast<T>(std::numeric_limits<T>::min() + 1 + step);
            } else {
                return static_cast<T>(1 + (which - 1) % (max - 2));
            }
        } else if constexpr (std::is_same_v<T, double>) {
            if (which < 2)
                return which == 0 ? 1.5 : -0.25;
            const double magnitude = static_cast<double>(which) + 0.5;
            return which % 2 == 0 ? magnitude : -magnitude;
        } else if constexpr (requires { text_form<T>::sample_texts[1]; }) {
            return text_form<T>::from_text(text_form<T>::sample_texts[which % 2]);
        } else {
            static_assert(unsampled<T>, "the wire codec writes a type with no sample");
        }
    }
};

template <>
struct sampler<std::string> {
    static std::string make(std::size_t which) {
        if (which < 2)
            return which == 0 ? std::string("a \"quoted\" back\\slash\nline \xc3\xa9") : "second";
        return "sample " + std::to_string(which);
    }
};

template <>
struct sampler<std::filesystem::path> {
    static std::filesystem::path make(std::size_t which) {
        if (which < 2)
            return which == 0 ? "dir/sub dir/file.txt" : "other";
        return "other " + std::to_string(which);
    }
};

template <>
struct sampler<std::chrono::sys_time<std::chrono::microseconds>> {
    static std::chrono::sys_time<std::chrono::microseconds> make(std::size_t which) {
        using namespace std::chrono;
        if (which < 2)
            return which == 0 ? sys_days{year{2026} / 3 / 14} + hours{15} + minutes{9} +
                                    seconds{26} + microseconds{535897} :
                                sys_days{year{1999} / 12 / 31} + hours{23} + minutes{59} +
                                    seconds{59};
        return sys_days{year{2000 + static_cast<int>(which % 200)} / 6 / 15} +
               microseconds{static_cast<long long>(which)};
    }
};

template <typename T>
struct sampler<std::optional<T>> {
    static std::optional<T> make(std::size_t which) {
        return sample<T>(which);
    }
};

template <typename T>
struct sampler<std::vector<T>> {
    static std::vector<T> make(std::size_t which) {
        if (which % 2 == 0)
            return {sample<T>(which), sample<T>(which + 1)};
        return {sample<T>(which)};
    }
};

template <typename T>
struct sampler<std::map<std::string, T>> {
    static std::map<std::string, T> make(std::size_t which) {
        if (which % 2 == 0)
            return {{"first", sample<T>(which)}, {"second", sample<T>(which + 1)}};
        return {{"only", sample<T>(which)}};
    }
};

}

template <typename T>
T sample(std::size_t which) {
    return detail::sampler<T>::make(which);
}

}

#endif
