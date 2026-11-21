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
#include <utility>

/**
 * @brief The C++ records and codec tables of one document family of XML
 * schemas, before clang-format: a port of ORE Studio's
 * generate_xml_bindings.py that names nothing of the project it serves.
 */
namespace compass::crosscheck::ops::xml_bindings {

/**
 * @brief What the generated files of every family of one project name and
 * the schemas do not: the header of the binding engine, the exact decimal
 * type and its header, the export macro of each accessor, and the element
 * names the schemas state wrongly, each as written and as read.
 */
struct project {
    std::string_view engine_include;
    std::string_view decimal_include;
    std::string_view decimal_type;
    std::string_view export_macro;
    std::span<const std::pair<std::string, std::string>> errata;
};

/**
 * @brief One run of the generator: one family, less the texts of its
 * schemas and its output paths. schema_names are what the banner of each
 * file says it was generated from. The include guard is include in upper
 * case, every character but a letter or digit an underscore.
 * exact_decimals names the real elements carried as exact decimals; "*"
 * names every real.
 */
struct family {
    std::string_view root_element;
    std::string_view root_type;
    std::string_view accessor;
    std::string_view name_space;
    std::string_view include;
    std::span<const std::string> schema_names;
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
 * @brief The files the generator writes for the family @p f of the project
 * @p p from the texts of @p schemas, read in that order.
 * @throws error where the script exits without writing.
 */
[[nodiscard]] output generate(std::span<const std::string> schemas,
    const family& f, const project& p);

}

#endif
