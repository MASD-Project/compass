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
#ifndef COMPASS_CODEC_OPS_TANGLE_HPP
#define COMPASS_CODEC_OPS_TANGLE_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.org/types/source_range.hpp"

namespace compass::codec::ops {

/**
 * @brief One file a document tangles: its path, relative to the root of the
 * checkout, and its bytes.
 */
struct tangle_target {
    std::string path;
    std::string content;
};

/**
 * @brief Why a block is not tangled.
 *
 * unsupported is a header argument, a switch or a value whose effect on the
 * bytes the codec does not reproduce. outside_checkout is a target that is
 * absolute or leaves the root of the checkout.
 */
enum class tangle_fault : std::uint8_t { unsupported, outside_checkout };

/**
 * @brief One reason a block is not tangled. range is the opening line of
 * the block, or the line that sets the header arguments.
 */
struct tangle_problem {
    compass::org::types::source_range range;
    tangle_fault fault = tangle_fault::unsupported;
    std::string message;
    std::string fix;
};

/**
 * @brief What tangling one document gives: the targets it writes, in the
 * order of their first block, and the problems that kept blocks from being
 * tangled. A target any of whose blocks has a problem is not written, so it
 * is not in targets.
 */
struct tangled_document {
    std::vector<tangle_target> targets;
    std::vector<tangle_problem> problems;
};

/**
 * @brief Tangles the source blocks of @p d as org-babel-tangle-file does
 * under emacs -Q.
 *
 * A source block with a language and a :tangle path is tangled; :tangle no
 * and a block without :tangle are not, nor is a block under a COMMENT
 * heading or a heading tagged ARCHIVE. The path is resolved against the
 * directory of the document. The bodies of the blocks of one target are
 * joined in document order, each followed by a newline, with an empty line
 * between two blocks.
 *
 * Header arguments that leave the bytes alone are accepted. Any other, a
 * switch, a #+header: line, and header-args set by #+property: or by a
 * property drawer are problems.
 */
[[nodiscard]] tangled_document tangle(const types::document& d);

/**
 * @brief The body a source block contributes to its target, given the text
 * between its opening and closing lines.
 *
 * Org's escaping comma is removed from each line that starts, after blanks,
 * with commas and then * or #+. The last newline is removed. The
 * indentation common to the non-blank lines is removed, a tab counting to
 * the next multiple of eight; a blank line with less loses its blanks, and
 * when a non-blank line has less nothing is removed. Last, the blanks and
 * line ends at both ends are removed.
 */
[[nodiscard]] std::string tangle_body(std::string_view raw);

}

#endif
