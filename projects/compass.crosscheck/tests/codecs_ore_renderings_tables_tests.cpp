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
#include "compass.crosscheck/codecs/ore_renderings_tables.hpp"

#include <array>
#include <map>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.store/ops/blob_hash.hpp"
#include "test_checkout.hpp"

namespace {

using namespace compass;
namespace ct = compass::codec::types;
using files = std::map<std::string, std::string, std::less<>>;

const std::string header =
    "projects/ores.ore/core/src/domain/conventions_renderings.hpp";

files generate(const files& all) {
    ct::checkout_files checkout;
    for (const auto& [path, bytes] : all)
        checkout.paths.push_back(path);
    checkout.content_of = [&](std::string_view p) {
        return std::optional(store::ops::blob_id_of(all.find(p)->second));
    };
    checkout.read = [&](std::string_view p) {
        return std::optional(all.find(p)->second);
    };
    ct::corpus corpus;
    corpus.files = &checkout;
    const org::types::type_registry types;
    ct::configuration configuration;
    configuration.set("ore_renderings_tables", "enabled", "true");
    const std::array codecs{crosscheck::codecs::ore_renderings_tables};
    const auto r = codec::ops::run(codec::ops::make_registry(codecs, types,
        configuration), corpus, types, 1);
    files out;
    const auto& f = r.files[0];
    for (std::uint32_t i = 0; i < f.size(); ++i)
        out.emplace(f.paths[ct::file_id{i}], f.contents[ct::file_id{i}]);
    return out;
}

}

TEST_CASE("each rendering maps to its canonical form in the file's order, "
    "quoted for C++", "[codecs][ore_renderings_tables]") {
    const auto out = generate({{"projects/ores.sql/utility/ore_renderings.json",
        R"({"description": "x", "tables": {
            "zeta": {"F": ["F", "Fol\"lo\\w"], "A": ["A"]},
            "alpha": {}}})"}});

    const auto& text = out.at(header);
    CHECK(text.find("\n * AUTO-GENERATED FILE - DO NOT EDIT MANUALLY\n") !=
        std::string::npos);
    CHECK(text.ends_with(" */\n\n"
        "#ifndef ORES_ORE_CORE_SRC_DOMAIN_CONVENTIONS_RENDERINGS_HPP\n"
        "#define ORES_ORE_CORE_SRC_DOMAIN_CONVENTIONS_RENDERINGS_HPP\n\n"
        "#include \"conventions_fields.hpp\"\n\n"
        "namespace ores::ore::domain::conventions_fields {\n\n"
        "const code_table zeta = {\n"
        "    {\"F\", \"F\"},\n"
        "    {\"Fol\\\"lo\\\\w\", \"F\"},\n"
        "    {\"A\", \"A\"},\n"
        "};\n\n"
        "const code_table alpha = {\n};\n\n"
        "}\n\n#endif\n"));
}

TEST_CASE("a renderings file that is not JSON writes nothing and is "
    "refused", "[codecs][ore_renderings_tables]") {
    const files broken{{"projects/ores.sql/utility/ore_renderings.json",
        "{\"tables\": "}};
    CHECK(generate(broken).empty());
    const auto r = crosscheck::tests::run(
        crosscheck::codecs::ore_renderings_tables, broken);
    REQUIRE(r.findings.size() == 1);
    CHECK(std::get<2>(r.findings[0]) == header + " is not generated: "
        "projects/ores.sql/utility/ore_renderings.json is not JSON");
}
