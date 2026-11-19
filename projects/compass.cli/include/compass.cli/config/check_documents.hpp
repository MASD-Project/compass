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
#ifndef COMPASS_CLI_CONFIG_CHECK_DOCUMENTS_HPP
#define COMPASS_CLI_CONFIG_CHECK_DOCUMENTS_HPP

#include <array>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/descriptor.hpp"
#include "compass.codec/types/exception.hpp"
#include "compass.store/io/git_objects.hpp"
#include "compass.store/types/git_object.hpp"

namespace compass::cli::config {

/**
 * @brief The directory of a checkout that holds its check documents, one
 * org file per check.
 */
inline constexpr std::string_view checks_directory = "compass/checks";

/**
 * @brief A check a project defines in a document, and the codec that runs
 * it. The descriptor's spans point into the check, so a check is never
 * moved once built.
 *
 * name is the document's file name without .org, and summary its
 * #+description:, or its path when it has none. definition is the text
 * its engine's parser reads, compass::cpp::ops::parse_forbid_definition
 * or parse_format_definition, made from the drawers of the document's
 * Definition heading; it is the value of the codec's one option, so
 * editing it runs the check again and editing the prose does not.
 */
struct check {
    std::string name;
    std::string path;
    std::string summary;
    std::string definition;
    std::vector<std::string> rule_names;
    std::vector<std::string> rule_summaries;
    std::array<codec::types::option, 1> options;
    std::vector<codec::types::rule> rules;
    codec::types::descriptor codec;

    check() = default;
    check(const check&) = delete;
    check& operator=(const check&) = delete;
};

/**
 * @brief The checks of a checkout, in file-name order, and the exceptions
 * their documents state.
 */
struct check_set {
    std::vector<std::unique_ptr<const check>> checks;
    codec::types::exception_set exceptions;
};

/**
 * @brief The check that the document @p text at @p path defines, with
 * the exceptions its table states added to @p exceptions, or the first
 * error in it as "<path>:<line>: <what>; fix: <how>".
 *
 * The level-1 heading "Definition" holds, in its property drawer, ENGINE,
 * forbid or format, and the settings every rule shares (FILES,
 * OUTSIDE). A format check states all of its definition there, has no
 * rule headings and no examples, and is a formatting check whose one
 * rule is unformatted. For a forbid check, each level-2 heading under it
 * is a rule: its title is the rule's name, its drawer holds the rule's
 * keys, and its first paragraph is the rule's summary, or its MESSAGE
 * when it has none. The first table
 * under the level-1 heading "Exceptions" has a row per exception, as the
 * configuration file's has, with a rule named without the check's name.
 *
 * Each source block under the level-1 heading "Examples" whose header
 * sets :compass is an example: :compass fires=<rule> or quiet, and :path
 * the file it stands for. The examples are part of the definition, so
 * editing one runs the check again, and the codec's last rule, example,
 * reports one that does not fire or stay quiet as it says.
 */
[[nodiscard]] std::expected<std::unique_ptr<const check>, std::string>
parse_check_document(std::string_view text, std::string_view path,
    codec::types::exception_set& exceptions);

/**
 * @brief The checks of the documents in checks_directory of @p checkout,
 * or the first error of one of them.
 */
[[nodiscard]] std::expected<check_set, std::string> read_check_documents(
    const std::filesystem::path& checkout);

/**
 * @brief The checks of the documents in checks_directory of the commit
 * @p commit, read from @p objects, or the first error of one of them.
 * @throws as store::io::list_commit does.
 */
[[nodiscard]] std::expected<check_set, std::string> read_check_documents(
    const store::io::git_objects& objects,
    const store::types::object_id& commit);

}

#endif
