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
#ifndef COMPASS_HTML_TYPES_RENDER_CONFIG_HPP
#define COMPASS_HTML_TYPES_RENDER_CONFIG_HPP

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace compass::html::types {

/**
 * @brief The URLs a project gives its documents and their files.
 *
 * link_template is the URL of a document, with {id} for the document's
 * :ID: and {anchor} for a heading in it. When a link names the document
 * itself, {anchor} is empty and a "#{anchor}" in the template is left out.
 * image_template is the URL of a file, with {path} for its path from the
 * root of the environment. Every value is percent-encoded before it is put
 * in; {path} keeps its slashes.
 */
struct render_config {
    std::string link_template = "/docs/{id}#{anchor}";
    std::string image_template = "/files/{path}";
};

/**
 * @brief What the renderer asks the caller about documents other than the
 * one it renders.
 *
 * document_of answers the :ID: of the document that holds an org ID, the
 * document's own or one of its headings'. document_at answers the :ID: of
 * the document at a path from the root of the environment. visible says
 * whether the caller may see a document, by its :ID:. An unset function
 * knows no document and sees none.
 *
 * A link to a document that is unknown or not visible renders as its text,
 * the same for both, so the output does not tell a reader that a document
 * they cannot see exists.
 */
struct link_resolver {
    std::function<std::optional<std::string>(std::string_view id)>
        document_of;
    std::function<std::optional<std::string>(std::string_view path)>
        document_at;
    std::function<bool(std::string_view document_id)> visible;
};

/**
 * @brief One document rendered.
 *
 * id, title and description are metadata: the :ID: of the document and the
 * values of its #+title: and #+description:, as plain text that is not
 * escaped. body is HTML in which every byte from the source is escaped.
 * unsupported names, once per occurrence, each element or object kind that
 * rendered as an org-unsupported fallback, as org-element names it.
 */
struct rendered_document {
    std::string id;
    std::string title;
    std::string description;
    std::string body;
    std::vector<std::string> unsupported;
};

}

#endif
