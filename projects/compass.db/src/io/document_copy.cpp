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
 * Template: masd_cpp_copy.cpp.mustache
 * To modify, update the template and regenerate.
 */
#include "compass.db/io/document_copy.hpp"

namespace compass::db::io {

void write_copy(ops::copy_writer& w,
    const store::types::blob_id& hash,
    const types::document_row& r) {
    w.field(hash);
    w.field(r.org_id);
    w.field(r.title);
    w.field(r.description);
    w.field(r.type);
    w.field(r.level);
    w.field(r.audience);
    w.field(r.created);
    w.field(r.updated);
    w.field(r.tags);
    w.field(r.keywords);
    w.field(r.properties);
    w.field(r.source);
    w.end();
}

}
