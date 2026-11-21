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
#ifndef COMPASS_CROSSCHECK_OPS_XML_BINDINGS_HPP
#define COMPASS_CROSSCHECK_OPS_XML_BINDINGS_HPP

#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

/**
 * @brief The C++ records and codec tables of one ORE document family, as
 * ORE Studio's generate_xml_bindings.py emits them from ORE's schemas,
 * before clang-format.
 */
namespace compass::crosscheck::ops::xml_bindings {

/**
 * @brief One run of the generator: its command line less the schemas and
 * the output paths. name_space and include default as the script's
 * --namespace and --include do when empty. exact_decimals names the real
 * elements carried as exact decimals; "*" names every real.
 */
struct family {
    std::string_view root_element;
    std::string_view root_type;
    std::string_view accessor;
    std::string_view name_space;
    std::string_view include;
    std::span<const std::string_view> exact_decimals;
};

/**
 * @brief The header and the source the generator writes.
 */
struct output {
    std::string header;
    std::string source;
};

/**
 * @brief Why the generator stops without writing: a schema that is not
 * XML, or a shape of one it refuses, as the script's message states it.
 */
class error final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief The files the generator writes for @p f from the texts of
 * @p schemas, read in that order.
 * @throws error where the script exits without writing.
 */
[[nodiscard]] output generate(std::span<const std::string> schemas,
    const family& f);

}

#endif
