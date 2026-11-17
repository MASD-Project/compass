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
#include "compass.codec/codecs/tangle.hpp"

#include "compass.codec/ops/tangle.hpp"
#include "compass.codec/types/corpus_view.hpp"

namespace compass::codec::codecs {

namespace {

void run(const types::document& d, const types::corpus_view& v,
    types::file_output& out) {
    if (!ops::in_directory(d.path, v.option("directory")))
        return;
    for (const auto& t : ops::tangle(d).targets)
        out.files.add(t.path, t.content);
}

}

const types::descriptor tangle{
    .name = "tangle",
    .version = 2,
    .summary = "Writes the files the source blocks of each document "
        "tangle to, as Emacs does.",
    .reads = types::inputs::every_document(),
    .options = ops::tangle_scope,
    .rules = {},
    .enabled_by_default = false,
    .run = types::document_function<types::file_output>{run}};

}
