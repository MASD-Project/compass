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
#ifndef COMPASS_HTML_OPS_RENDER_HPP
#define COMPASS_HTML_OPS_RENDER_HPP

#include <string_view>
#include "compass.html/types/render_config.hpp"
#include "compass.org/types/tables.hpp"

namespace compass::html::ops {

/**
 * @brief Renders one parsed document as HTML.
 *
 * @p source is the text @p t was parsed from. @p path is the document's
 * path from the root of the environment; relative file links resolve
 * against its directory, and a file link that leaves the root renders as
 * text.
 *
 * Every byte taken from the source is escaped and no markup the source
 * writes passes through: an html export block or export snippet renders
 * its content as escaped text, and #+attr_html: is ignored. The output
 * needs no sanitiser. Export blocks and snippets for other backends,
 * comments, commented headings and headings tagged noexport are left out,
 * as org's HTML export leaves them out, and so are drawers and property
 * drawers.
 * Any other element or object the renderer does not support renders as its
 * escaped source in an org-unsupported div or span.
 */
[[nodiscard]] types::rendered_document render(std::string_view source,
    const org::types::tables_view& t, std::string_view path,
    const types::render_config& config, const types::link_resolver& links);

}

#endif
