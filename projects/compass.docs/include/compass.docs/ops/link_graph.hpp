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
#ifndef COMPASS_DOCS_OPS_LINK_GRAPH_HPP
#define COMPASS_DOCS_OPS_LINK_GRAPH_HPP

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"

namespace compass::docs::ops {

/**
 * @brief No level-1 section: a link before the first heading.
 */
inline constexpr std::uint32_t no_section =
    std::numeric_limits<std::uint32_t>::max();

/**
 * @brief One bracket id link of a page, as org parses it, whose target
 * is shaped like an ID: 36 hexadecimal digits and hyphens.
 *
 * line is the byte its line starts at. target is the path as written,
 * trimmed; label the description as written, empty when there is none.
 * section indexes the page's level-1 sections, or is no_section. ordering
 * is true before the page's first level-1 "What this cluster does not
 * cover" or "See also" section; aside is true when the section the link
 * sits in, up to the next heading of any level, holds an "# aside"
 * comment.
 */
struct id_link {
    std::uint32_t line = 0;
    std::string_view target;
    std::string_view label;
    bool described = false;
    std::uint32_t section = no_section;
    bool ordering = false;
    bool aside = false;
};

/**
 * @brief A "# BEGIN generated X (script)" comment: the byte its line
 * starts at and the script it names.
 */
struct marker {
    std::uint32_t line = 0;
    std::string_view script;
};

/**
 * @brief What the link graph knows of one page.
 *
 * utf8 is false for a page that is not UTF-8. type is the first word of
 * its first #+type: keyword, empty when it has none; title its first
 * #+title:. structure_note is true when its first #+filetags: holds the
 * tag structure_note, and evidence when the page is evidence. ids holds
 * every :ID: property of its drawers, in upper case and in order, so the
 * first is the page's own when it has one. sections holds the titles of
 * its level-1 headings in order. see_also is the byte of its level-1 See
 * also heading, or 0.
 */
struct page {
    std::string_view path;
    bool utf8 = true;
    std::string_view type;
    std::string_view title;
    bool structure_note = false;
    bool evidence = false;
    std::uint32_t see_also = 0;
    std::vector<std::string_view> ids;
    std::vector<std::string_view> sections;
    std::vector<id_link> links;
    std::vector<marker> markers;
};

/**
 * @brief The fact of @p d, the bytes page_of reads back.
 */
[[nodiscard]] std::string fact_of(const compass::codec::types::document& d);

/**
 * @brief The page @p fact describes, its strings pointing into @p fact.
 */
[[nodiscard]] page page_of(std::string_view path, std::string_view fact);

/**
 * @brief @p s in upper case, ASCII letters only.
 */
[[nodiscard]] std::string upper(std::string_view s);

}

#endif
