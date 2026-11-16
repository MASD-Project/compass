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
#include "compass.sql/codecs/handler_permissions.hpp"

#include <catch2/catch_test_macros.hpp>
#include "test_checkout.hpp"

namespace {

using namespace compass::sql;
using tests::files;
using place = std::pair<std::string, std::uint32_t>;
using places = std::vector<place>;

files tree() {
    return {
        {"projects/ores.sql/populate/iam/iam_permissions_populate.sql",
         "select upsert('refdata::books:read');\n"
         "select upsert('refdata::rates:write');\n"},
        {"projects/ores.refdata/core/include/ores.refdata/messaging/"
         "book_handler.hpp",
         "if (!has_permission(ctx, \"refdata::books:read\")) return;\n"
         "if (!has_permission(ctx, \"refdata::books:write\")) return;\n"},
        {"projects/ores.refdata/core/src/messaging/rate_handler.cpp",
         "if (!has_permission(ctx, \"refdata::rates:\" + verb)) return;\n"
         "if (!has_permission(ctx, \"refdata::fx:\" + verb)) return;\n"},
        {"projects/ores.refdata/core/src/other/x_handler.cpp",
         "has_permission(ctx, \"refdata::hidden:read\");\n"},
        {"projects/ores.unlisted/core/src/messaging/y_handler.cpp",
         "has_permission(ctx, \"unlisted::y:read\");\n"}};
}

}

TEST_CASE("a code a handler checks and no seed defines is reported",
    "[codecs][handler_permissions]") {
    const auto r = tests::run(codecs::handler_permissions, tree());
    CHECK(tests::places(r.findings) == places{
        {"projects/ores.refdata/core/include/ores.refdata/messaging/"
         "book_handler.hpp", 2},
        {"projects/ores.refdata/core/src/messaging/rate_handler.cpp", 2}});
}
