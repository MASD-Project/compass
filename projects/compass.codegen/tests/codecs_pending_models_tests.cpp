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
#include "compass.codegen/codecs/pending_models.hpp"

#include <array>
#include <map>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace {

using namespace compass;
namespace ct = compass::codec::types;
using files = std::map<std::string, std::string, std::less<>>;

/**
 * @brief The files pending_models writes over @p all, by path, and the
 * message of each output it refuses, in @p refusals when given.
 */
files generate(const files& all,
    std::vector<std::string>* refusals = nullptr) {
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
    configuration.set("pending_models", "enabled", "true");
    const std::array codecs{codegen::codecs::pending_models};
    const auto r = codec::ops::run(codec::ops::make_registry(codecs, types,
        configuration), corpus, types, 2);
    files out;
    const auto& f = r.files[0];
    for (std::uint32_t i = 0; i < f.size(); ++i)
        out.emplace(f.paths[ct::file_id{i}], f.contents[ct::file_id{i}]);
    const auto& d = r.diagnostics[0];
    for (std::uint32_t i = 0; i < d.size() && refusals != nullptr; ++i)
        refusals->emplace_back(d.messages[ct::diagnostic_id{i}]);
    return out;
}

/**
 * @brief A gated entity and a gated junction. The expected texts are what
 * ORE's codegen.pending_change derives from them.
 */
files tree() {
    return {
        {"projects/ores.a/modeling/ores.a.thing.org",
         ":PROPERTIES:\n"
         ":ores.cpp.wire-codec.enabled: true\n"
         ":ID: T\n"
         ":END:\n"
         "#+title: ores.a.thing\n"
         "#+component: a\n"
         "#+entity_singular: thing\n"
         "#+entity_plural: things\n"
         "#+entity_title: Thing\n"
         "\n"
         "* Flags\n"
         ":PROPERTIES:\n"
         ":gated:   true\n"
         ":profile: simple-lookup\n"
         ":parent_id_column: parent\n"
         ":END:\n"
         "\n"
         "* Columns\n"
         "\n"
         "** id\n"
         ":PROPERTIES:\n"
         ":type: uuid\n"
         ":primary_key: true\n"
         ":END:\n"
         "\n"
         "The id.\n"
         "\n"
         "** code\n"
         ":PROPERTIES:\n"
         ":type: text\n"
         ":natural_key: true\n"
         ":END:\n"
         "\n"
         "#+begin_src cpp :name generator\n"
         "uuid_gen()\n"
         "#+end_src\n"
         "\n"
         "* Foreign keys\n"
         "\n"
         "** other_id\n"
         ":PROPERTIES:\n"
         ":table: ores_a_others_tbl\n"
         ":list_by: true\n"
         ":END:\n"
         "\n"
         "** gated_id\n"
         ":PROPERTIES:\n"
         ":table: ores_a_links_tbl\n"
         ":END:\n"
         "\n"
         "* Insert trigger\n"
         "\n"
         "** Validations\n"
         "\n"
         "| column | validation_function | max_rows |\n"
         "|--------+---------------------+----------|\n"
         "| code   | check_code_fn       | 3        |\n"
         "\n"
         "** Other\n"
         "\n"
         "* C++\n"
         "\n"
         "** Flags\n"
         ":PROPERTIES:\n"
         ":x: y\n"
         ":END:\n"
         "\n"
         "** Entity includes\n"
         "\n"
         "#+begin_src cpp\n"
         "#include <e>\n"
         "#+end_src\n"
         "\n"
         "** Domain includes\n"
         "\n"
         "#+begin_src cpp\n"
         "#include <d>\n"
         "#+end_src\n"},
        {"projects/ores.a/modeling/ores.a.link.org",
         "#+title: ores.a.link\n"
         "#+type: ores.codegen.junction\n"
         "#+component: a\n"
         "#+name_singular: link\n"
         "#+name: links\n"
         "#+name_title: Link\n"
         "\n"
         "* Flags\n"
         ":PROPERTIES:\n"
         ":pending_change: true\n"
         ":END:\n"
         "\n"
         "* Left\n"
         ":PROPERTIES:\n"
         ":column:   thing_id\n"
         ":type:     uuid\n"
         ":cpp_type: boost::uuids::uuid\n"
         ":END:\n"
         "\n"
         "The thing.\n"
         "\n"
         "More.\n"
         "\n"
         "* Right\n"
         ":PROPERTIES:\n"
         ":column:   other_id\n"
         ":type:     uuid\n"
         ":cpp_type: boost::uuids::uuid\n"
         ":END:\n"
         "The other.\n"}};
}

}

TEST_CASE("a gated entity and a gated junction get the pending models ORE "
    "derives", "[codecs][pending_models]") {
    const auto out = generate(tree());

    REQUIRE(out.size() == 2);
    {
        const auto& text = out.at("projects/ores.a/modeling/ores.a.thing_change.org");
        CHECK(text.starts_with(
            ":PROPERTIES:\n"
            ":ores.cpp.wire-codec.enabled: true\n"
            ":ID: B4DF287D-DB0E-59E7-BF74-75611E1DBF10\n"
            ":END:\n"
            "#+title: ores.a.thing_change\n"
            "#+description: GENERATED from ores.a.thing by derive_pending_models.py. One proposed write to the thing, held until its request is decided.\n"
            "#+type: ores.codegen.entity\n"
            "#+component: a\n"
            "#+filetags: :model:entity:a:pending:\n"
            "#+brief: One proposed write to a thing, held for its request.\n"
            "#+entity_singular: thing_change\n"
            "#+entity_plural: thing_changes\n"
            "#+entity_title: Thing Change\n"
            "#+created: 2026-10-10\n"
            "#+updated: 2026-10-10\n"
            "\n"
            "Do not edit. Change ores.a.thing and run\n"
            "derive_pending_models.py.\n"
            "\n"
            "* Flags\n"
            ":PROPERTIES:\n"
            ":profile:   uuid-identified-lookup\n"
            ":client_read_only: true\n"
            ":END:\n"
            "\n"));
        CHECK(text.ends_with(
            "** other_id\n"
            ":PROPERTIES:\n"
            ":table: ores_a_others_tbl\n"
            ":END:\n"
            "\n"
            "\n"
            "\n"
            "* Insert trigger\n"
            "\n"
            "** Validations\n"
            "\n"
            "| column | validation_function |\n"
            "|--------+---------------------|\n"
            "| code | check_code_fn |\n"
            "\n"
            "* C++\n"
            "\n"
            "** Flags\n"
            ":PROPERTIES:\n"
            ":subcomponent:    api\n"
            ":END:\n"
            "\n"
            "** Repository\n"
            ":PROPERTIES:\n"
            ":entity_singular_short: thing_change\n"
            ":entity_plural_short:   thing_changes\n"
            ":entity_singular_words: thing change\n"
            ":entity_plural_words:   thing changes\n"
            ":END:\n"
            "\n"
            "** Domain includes\n"
            "\n"
            "#+begin_src cpp\n"
            "#include <d>\n"
            "#+end_src\n"
            "\n"
            "** Entity includes\n"
            "\n"
            "#+begin_src cpp\n"
            "#include <e>\n"
            "#+end_src\n"
            "\n"));
    }
    {
        const auto& text = out.at("projects/ores.a/modeling/ores.a.link_change.org");
        CHECK(text.starts_with(
            ":PROPERTIES:\n"
            ":ID: E0B88367-61AD-5008-A672-B0AB24AD83B1\n"
            ":END:\n"
            "#+title: ores.a.link_change\n"
            "#+description: GENERATED from ores.a.link by derive_pending_models.py. One proposed write to the link, held until its request is decided.\n"
            "#+type: ores.codegen.entity\n"
            "#+component: a\n"
            "#+filetags: :model:entity:a:pending:\n"
            "#+brief: One proposed write to a link, held for its request.\n"
            "#+entity_singular: link_change\n"
            "#+entity_plural: link_changes\n"
            "#+entity_title: Link Change\n"
            "#+created: 2026-10-10\n"
            "#+updated: 2026-10-10\n"
            "\n"
            "Do not edit. Change ores.a.link and run\n"
            "derive_pending_models.py.\n"
            "\n"
            "* Flags\n"
            ":PROPERTIES:\n"
            ":schema:               public\n"
            ":product:              ores\n"
            ":component:             a\n"
            ":profile:   uuid-identified-lookup\n"
            ":client_read_only: true\n"
            ":END:\n"
            "\n"));
        CHECK(text.ends_with(
            "* C++\n"
            "\n"
            "** Flags\n"
            ":PROPERTIES:\n"
            ":subcomponent:    api\n"
            ":END:\n"
            "\n"
            "** Repository\n"
            ":PROPERTIES:\n"
            ":entity_singular_short: link_change\n"
            ":entity_plural_short:   link_changes\n"
            ":entity_singular_words: link change\n"
            ":entity_plural_words:   link changes\n"
            ":END:\n"
            "\n"
            "** Domain includes\n"
            "\n"
            "#+begin_src cpp :name includes\n"
            "#include <string>\n"
            "#include <boost/uuid/uuid.hpp>\n"
            "#+end_src\n"
            "\n"));
    }
}

TEST_CASE("a model the derivation refuses is reported, and the others are "
    "written", "[codecs][pending_models]") {
    auto all = tree();
    auto& thing = all.at("projects/ores.a/modeling/ores.a.thing.org");
    thing.replace(thing.find(":profile:"), 9, ":profiles");

    std::vector<std::string> refusals;
    const auto out = generate(all, &refusals);
    CHECK(out.size() == 1);
    CHECK(out.contains("projects/ores.a/modeling/ores.a.link_change.org"));
    CHECK(refusals == std::vector<std::string>{
        "projects/ores.a/modeling/ores.a.thing_change.org is not generated: "
        "projects/ores.a/modeling/ores.a.thing.org: gated model has no "
        "':profile:' flag"});
}
