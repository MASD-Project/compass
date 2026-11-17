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

#include <array>
#include <map>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "compass.codec/codecs/tangle_check.hpp"
#include "compass.codec/ops/outputs.hpp"
#include "compass.codec/ops/registry.hpp"
#include "compass.codec/ops/run.hpp"
#include "compass.codec/ops/tangle.hpp"
#include "test_corpus.hpp"

namespace {

using namespace compass::codec;

struct tangle_run {
    types::registry registry;
    types::run_result result;

    [[nodiscard]] std::map<std::string, std::string> files() const {
        std::map<std::string, std::string> out;
        const auto& f = result.files[0];
        for (std::uint32_t i = 0; i < f.size(); ++i)
            out.emplace(f.paths[types::file_id{i}],
                f.contents[types::file_id{i}]);
        return out;
    }

    [[nodiscard]] const types::diagnostic_table& diagnostics() const {
        return result.diagnostics[1];
    }
};

tangle_run run_tangle(const tests::test_corpus& t,
    std::string_view directory = {}) {
    const std::array codecs{codecs::tangle, codecs::tangle_check};
    types::configuration settings;
    settings.set("tangle", "enabled", "true");
    settings.set("tangle_check", "enabled", "true");
    settings.set("tangle", "directory", directory);
    settings.set("tangle_check", "directory", directory);
    tangle_run r{ops::make_registry(codecs, t.types(), settings), {}};
    r.result = ops::run(r.registry, t.corpus(), t.types(), 2);
    return r;
}

}

TEST_CASE("blocks of one target are joined in document order with an "
    "empty line between them", "[codecs][tangle]") {
    tests::test_corpus t;
    t.add("lib/doc.org",
        "* One\n"
        "#+begin_src mustache :tangle a.mustache\nfirst\n#+end_src\n"
        "#+begin_src mustache :tangle b.mustache\nother\n#+end_src\n"
        "* Two\n"
        "#+begin_src mustache :tangle a.mustache\nsecond\nlines\n"
        "#+end_src\n"
        "#+begin_src mustache :tangle a.mustache\nthird\n#+end_src\n");

    const auto files = run_tangle(t).files();

    REQUIRE(files.size() == 2);
    CHECK(files.at("lib/a.mustache") ==
        "first\n\nsecond\nlines\n\nthird\n");
    CHECK(files.at("lib/b.mustache") == "other\n");
}

TEST_CASE("an empty block still takes its line and its padline",
    "[codecs][tangle]") {
    tests::test_corpus t;
    t.add("doc.org",
        "#+begin_src text :tangle a.out\nx\n#+end_src\n"
        "#+begin_src text :tangle a.out\n#+end_src\n"
        "#+begin_src text :tangle a.out\nlast\n\n\n#+end_src\n");

    CHECK(run_tangle(t).files().at("a.out") == "x\n\n\n\nlast\n");
}

TEST_CASE("the escaping comma goes from lines starting with ,* and ,#+ "
    "only", "[codecs][tangle]") {
    tests::test_corpus t;
    t.add("doc.org",
        "#+begin_src org :tangle a.org\n"
        "start\n"
        ",* heading\n"
        ",#+begin_src sh\n"
        "  ,#+end_src\n"
        ",,#+kept_one\n"
        ",#not_escaped\n"
        "a,* mid line\n"
        "#+end_src\n");

    CHECK(run_tangle(t).files().at("a.org") ==
        "start\n"
        "* heading\n"
        "#+begin_src sh\n"
        "  #+end_src\n"
        ",#+kept_one\n"
        ",#not_escaped\n"
        "a,* mid line\n");
}

TEST_CASE("a target is resolved against the directory of its document",
    "[codecs][tangle]") {
    tests::test_corpus t;
    t.add("lib/templates/doc.org",
        "#+begin_src text :tangle ../out/a.txt\na\n#+end_src\n"
        "#+begin_src text :tangle ./sub//b.txt\nb\n#+end_src\n");
    t.add("top.org", "#+begin_src text :tangle c.txt\nc\n#+end_src\n");

    const auto files = run_tangle(t).files();

    CHECK(files.size() == 3);
    CHECK(files.contains("lib/out/a.txt"));
    CHECK(files.contains("lib/templates/sub/b.txt"));
    CHECK(files.contains("c.txt"));
}

TEST_CASE("blocks with :tangle no or without :tangle, or under a COMMENT or "
    "ARCHIVE heading are not tangled", "[codecs][tangle]") {
    tests::test_corpus t;
    t.add("doc.org",
        "#+begin_src text :tangle no\nno\n#+end_src\n"
        "#+begin_src text\nabsent\n#+end_src\n"
        "* COMMENT Hidden\n"
        "** Below\n"
        "#+begin_src text :tangle hidden.txt\nh\n#+end_src\n"
        "* Old :ARCHIVE:\n"
        "#+begin_src text :tangle archived.txt\nh\n#+end_src\n"
        "* Live\n"
        "#+begin_src text :tangle live.txt :exports none :results silent\n"
        "live\n#+end_src\n");

    const auto r = run_tangle(t);
    const auto files = r.files();

    REQUIRE(files.size() == 1);
    CHECK(files.at("live.txt") == "live\n");
    CHECK(r.diagnostics().size() == 0);
}

TEST_CASE("an unimplemented header argument is reported and its target is "
    "not written", "[codecs][tangle]") {
    tests::test_corpus t;
    t.add("doc.org",
        "#+begin_src sh :tangle run.sh :shebang #!/bin/sh\necho\n#+end_src\n"
        "#+begin_src sh :tangle run.sh\nmore\n#+end_src\n"
        "#+begin_src sh -i :tangle switch.sh\nx\n#+end_src\n"
        "#+begin_src sh :tangle fine.sh :noweb no :padline yes\nok\n"
        "#+end_src\n");

    const auto r = run_tangle(t);
    const auto files = r.files();

    REQUIRE(files.size() == 1);
    CHECK(files.contains("fine.sh"));
    const auto& d = r.diagnostics();
    REQUIRE(d.size() == 2);
    CHECK(d.messages[types::diagnostic_id{0}] ==
        "source block tangling to run.sh sets :shebang #!/bin/sh, which "
        "tangling does not implement");
    CHECK(d.messages[types::diagnostic_id{1}] ==
        "source block tangling to switch.sh has the switch -i, which "
        "tangling does not implement");
    CHECK(d.rules[types::diagnostic_id{0}] == types::rule_id{0});
}

TEST_CASE("header arguments set outside the opening line block every "
    "target of the document", "[codecs][tangle]") {
    tests::test_corpus t;
    t.add("prop.org",
        "#+property: header-args :padline no\n"
        "#+begin_src text :tangle a.txt\na\n#+end_src\n");
    t.add("header.org",
        "#+header: :padline no\n"
        "#+begin_src text :tangle b.txt\nb\n#+end_src\n");

    const auto r = run_tangle(t);

    CHECK(r.files().empty());
    CHECK(r.diagnostics().size() == 2);
}

TEST_CASE("a target outside the checkout is reported",
    "[codecs][tangle]") {
    tests::test_corpus t;
    t.add("doc/a.org",
        "#+begin_src text :tangle ../../up.txt\nx\n#+end_src\n"
        "#+begin_src text :tangle /abs.txt\nx\n#+end_src\n");

    const auto r = run_tangle(t);

    CHECK(r.files().empty());
    REQUIRE(r.diagnostics().size() == 2);
    CHECK(r.diagnostics().rules[types::diagnostic_id{0}] ==
        types::rule_id{1});
}

TEST_CASE("two documents that tangle to one target are refused",
    "[codecs][tangle]") {
    tests::test_corpus t;
    t.add("a.org", "#+begin_src text :tangle out/x.txt\na\n#+end_src\n");
    t.add("out/b.org", "#+begin_src text :tangle x.txt\nb\n#+end_src\n");

    const auto r = run_tangle(t);
    const auto problems =
        ops::check_outputs(ops::outputs_of(r.registry, r.result));

    REQUIRE(problems.size() == 1);
    CHECK(problems[0] == "tangle and tangle both produced \"out/x.txt\"");
}

TEST_CASE("common indentation goes as org-remove-indentation removes it",
    "[codecs][tangle]") {
    // The expected bytes are what Emacs 30.2 with Org 9.7.11 tangled.
    CHECK(ops::tangle_body(
        "  a\n          b\n      \n    \n   c\n\td\n") ==
        "a\n        b\n    \n  \n c\n\td");
    CHECK(ops::tangle_body("    a\n      b\n  c\n        d\n") ==
        "a\n    b\nc\n      d");
    CHECK(ops::tangle_body("  x\n\n   \n            y\n,#not\n") ==
        "x\n\n   \n            y\n,#not");
    CHECK(ops::tangle_body("  a\n \n") == "a");
    CHECK(ops::tangle_body("\n\n   \n") == "");
}

TEST_CASE("the directory option tangles only the documents directly in it",
    "[codecs][tangle]") {
    tests::test_corpus t;
    t.add("lib/templates/a.org",
        "#+begin_src text :tangle a.txt\na\n#+end_src\n");
    t.add("lib/templates/deeper/b.org",
        "#+begin_src text :tangle b.txt\nb\n#+end_src\n");
    t.add("lib/c.org", "#+begin_src text :tangle c.txt :wrap x\nc\n"
        "#+end_src\n");

    const auto r = run_tangle(t, "lib/templates");

    CHECK(r.files() == std::map<std::string, std::string>{
        {"lib/templates/a.txt", "a\n"}});
    CHECK(r.diagnostics().size() == 0);
}
