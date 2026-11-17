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
#ifndef COMPASS_CPP_OPS_SOURCE_MODEL_HPP
#define COMPASS_CPP_OPS_SOURCE_MODEL_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "compass.cpp/ops/lexer.hpp"

/**
 * @brief What one C++ source declares and names, read from its lexed
 * code. Code an #if 0 excludes is not read.
 */
namespace compass::cpp::ops {

/**
 * @brief An #include directive: its header name, without its quotes or
 * angle brackets, and where its # is.
 */
struct include_directive {
    std::string_view target;
    bool angled = false;
    std::uint32_t offset = 0;
};

/**
 * @brief A #define: the macro's name and its body, the code after the
 * name (and after its parameters, for a function-like macro) with splices
 * as blanks.
 */
struct macro_definition {
    std::string_view name;
    std::string_view body;
    bool function_like = false;
    std::uint32_t offset = 0;
};

enum class declaration_kind : std::uint8_t {
    /** A class, struct or union, or a scoped enum. */
    type,
    /** An unscoped enum. */
    enumeration,
    function,
    variable
};

/**
 * @brief A declaration at namespace scope, as written: scope holds the
 * enclosing named namespaces, joined by ::; anonymous is true inside an
 * anonymous namespace. macro is the identifier between a class key and
 * the name, as in class ORES_X_EXPORT name, and empty when there is none.
 * The specifier flags are those written before the declared type.
 * is_enum is true for a scoped enum. qualified is true when the name
 * follows ::, as in a definition of a::f. offset is where the name is;
 * start where the declaration is, after any template head.
 */
struct declaration {
    declaration_kind kind = declaration_kind::variable;
    std::string scope;
    std::string_view name;
    std::string_view macro;
    bool definition = false;
    bool anonymous = false;
    bool is_static = false;
    bool is_inline = false;
    bool is_const = false;
    bool is_constexpr = false;
    bool is_extern = false;
    bool is_template = false;
    bool is_enum = false;
    bool qualified = false;
    std::uint32_t offset = 0;
    std::uint32_t start = 0;
};

/**
 * @brief The head of a class, struct, union or enum at any scope: where
 * its key is, the key (class, struct, union or enum, for enum class too),
 * the identifier after the key and any attributes when that is not the
 * name (a macro, as in class ORES_X_EXPORT name), and the name. An enum
 * head ends with :, { or ;, and using enum is not one.
 */
struct type_head {
    std::uint32_t offset = 0;
    std::string_view key;
    std::string_view macro;
    std::string_view name;
};

/**
 * @brief A name the code writes: an identifier, or identifiers joined by
 * ::, with a leading :: when written. text is the code that spells it,
 * spaces included. called is true when ( follows it.
 */
struct name_use {
    std::string_view text;
    std::uint32_t offset = 0;
    bool called = false;
};

/**
 * @brief The identifiers of a name, in order.
 */
[[nodiscard]] std::vector<std::string_view> components_of_name(
    std::string_view text);

struct source_model {
    std::vector<include_directive> includes;
    std::vector<macro_definition> macros;
    std::vector<declaration> declarations;
    std::vector<type_head> heads;
    std::vector<name_use> names;
};

/**
 * @brief The model of @p source, which @p l lexes. Its views point into
 * both, which must outlive it.
 */
[[nodiscard]] source_model model_of(std::string_view source, const lexed& l);

}

#endif
