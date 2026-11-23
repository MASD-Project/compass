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
 * Template: masd_cpp_http_service_binding.hpp.mustache
 * To modify, update the template and regenerate.
 */
#ifndef COMPASS_SERVE_NET_BINDING_HPP
#define COMPASS_SERVE_NET_BINDING_HPP

#include "compass.serve/domain/http_request.hpp"
#include "compass.serve/domain/refusal.hpp"
#include "compass.db/codec/json_codec.hpp"
#include "compass.db/codec/text_form.hpp"
#include <iterator>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace compass::serve::net {

/**
 * @brief A request's query parameters in the order the target states them,
 * each name and value percent-decoded, a plus read as a space.
 */
[[nodiscard]] std::vector<std::pair<std::string, std::string>>
query_parameters(std::string_view target);

/**
 * @brief The JSON Schema @p schema with only the properties @p members
 * names, and only those of them it requires: a request's schema narrowed to
 * the members its body carries.
 */
[[nodiscard]] std::string body_schema(std::string_view schema,
    std::initializer_list<std::string_view> members);

namespace binding_detail {

template <typename T>
struct is_vector : std::false_type {};
template <typename T>
struct is_vector<std::vector<T>> : std::true_type {};

template <typename T>
struct is_optional : std::false_type {};
template <typename T>
struct is_optional<std::optional<T>> : std::true_type {};

/// A message of two texts, a name and a value, written name:value.
template <typename T>
concept name_value = requires(T t) {
    { t.name } -> std::convertible_to<std::string>;
    { t.value } -> std::convertible_to<std::string>;
};

}

/**
 * @brief Builds a request message from a request's path, query and body by
 * the rules every route shares, refusing as invalid what they do not allow.
 *
 * A scalar parameter is given at most once; a vector is the parameter
 * repeated; a message of a name and a value is written name:value and split
 * at the first colon. finish() refuses every parameter nothing read, so an
 * unknown parameter is never silently dropped.
 */
class binding final {
public:
    explicit binding(const domain::http_request& request);

    /// Sets @p into from the path parameter @p name, which the route matched.
    template <typename T>
    void path(std::string_view name, T& into) const {
        into = convert<T>(name, request_.get_path_param(std::string(name)));
    }

    /**
     * @brief Sets @p into from the query parameter @p name, leaving it as it
     * is when absent and not @p required.
     */
    template <typename T>
    void query(std::string_view name, T& into, bool required) {
        using binding_detail::is_optional;
        using binding_detail::is_vector;
        auto given = take(name);
        if (given.empty()) {
            if (required)
                throw domain::refusal::invalid("The query parameter " +
                    std::string(name) + " is required.");
            return;
        }
        if constexpr (is_vector<T>::value) {
            into.clear();
            for (const auto& v : given)
                into.push_back(convert<typename T::value_type>(name, v));
        } else {
            if (given.size() > 1)
                throw domain::refusal::invalid("The query parameter " +
                    std::string(name) + " is given more than once.");
            if constexpr (is_optional<T>::value)
                into = convert<typename T::value_type>(name, given.front());
            else
                into = convert<T>(name, given.front());
        }
    }

    /// Refuses @p value unless it is one of @p allowed.
    static void one_of(std::string_view name, std::string_view value,
        std::initializer_list<std::string_view> allowed);

    /**
     * @brief Reads the JSON body into @p into: an object of no member but
     * @p members, with every one of @p required. The members it leaves out
     * keep @p into's values, so the path and the query set the rest.
     */
    template <typename T>
    void body(T& into, std::initializer_list<std::string_view> members,
        std::initializer_list<std::string_view> required) const {
        check_body(members, required);
        auto read = compass::db::codec::json::read_lenient<T>(
            request_.body.empty() ? std::string_view("{}")
                                  : std::string_view(request_.body));
        if (!read)
            throw domain::refusal::invalid("The body is not a valid request: " +
                read.error());
        into = std::move(*read);
    }

    /// Refuses the parameters no call read.
    void finish() const;

private:
    [[nodiscard]] std::vector<std::string> take(std::string_view name);

    void check_body(std::initializer_list<std::string_view> members,
        std::initializer_list<std::string_view> required) const;

    template <typename T>
    static T convert(std::string_view name, const std::string& text) {
        if constexpr (std::is_same_v<T, std::string>) {
            return text;
        } else if constexpr (std::is_same_v<T, bool>) {
            if (text == "true")
                return true;
            if (text == "false")
                return false;
            throw domain::refusal::invalid("The parameter " + std::string(name) +
                " is true or false, not " + text + ".");
        } else if constexpr (std::is_integral_v<T>) {
            T v{};
            const auto* end = text.data() + text.size();
            const auto [at, ec] = std::from_chars(text.data(), end, v);
            if (ec != std::errc() || at != end || text.empty())
                throw domain::refusal::invalid("The parameter " + std::string(name) +
                    " is a whole number in range, not " + text + ".");
            return v;
        } else if constexpr (std::is_enum_v<T>) {
            using form = compass::db::codec::text_form<T>;
            for (std::size_t i = 0; i < std::size(form::names); ++i)
                if (form::names[i] == text)
                    return form::values[i];
            std::string names;
            for (const auto n : form::names)
                names += (names.empty() ? "" : ", ") + std::string(n);
            throw domain::refusal::invalid("The parameter " + std::string(name) + " is one of " +
                                           names + ", not " + text + ".");
        } else if constexpr (binding_detail::name_value<T>) {
            const auto colon = text.find(':');
            if (colon == std::string::npos)
                throw domain::refusal::invalid("The parameter " + std::string(name) +
                    " is written name:value, not " + text + ".");
            T v;
            v.name = text.substr(0, colon);
            v.value = text.substr(colon + 1);
            return v;
        } else {
            static_assert(sizeof(T) == 0, "No binding converts a parameter to this type.");
        }
    }

    const domain::http_request& request_;
    std::vector<std::pair<std::string, std::string>> parameters_;
    std::vector<bool> read_;
};

}

#endif
