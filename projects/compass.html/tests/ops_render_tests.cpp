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
#include "compass.html/ops/render.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "compass.ingest/org/parser.hpp"

namespace {

using Catch::Matchers::ContainsSubstring;
using compass::html::types::link_resolver;
using compass::html::types::render_config;
using compass::html::types::rendered_document;

/**
 * @brief A corpus of three documents: DOC-A, with heading HEAD-A1, and
 * DOC-B are visible; DOC-HIDDEN is not.
 */
link_resolver corpus() {
    link_resolver r;
    r.document_of = [](std::string_view id) -> std::optional<std::string> {
        static const std::map<std::string, std::string, std::less<>> ids = {
            {"DOC-A", "DOC-A"}, {"HEAD-A1", "DOC-A"}, {"DOC-B", "DOC-B"},
            {"DOC-HIDDEN", "DOC-HIDDEN"}};
        const auto it = ids.find(id);
        if (it == ids.end())
            return std::nullopt;
        return it->second;
    };
    r.document_at = [](std::string_view path) -> std::optional<std::string> {
        if (path == "doc/other/b.org")
            return "DOC-B";
        if (path == "doc/secret.org")
            return "DOC-HIDDEN";
        return std::nullopt;
    };
    r.visible = [](std::string_view id) { return id != "DOC-HIDDEN"; };
    return r;
}

rendered_document rendered(std::string_view source) {
    compass::org::types::tables t;
    compass::ingest::org::parse(source, t);
    render_config config;
    config.link_template = "/docs/{id}#{anchor}";
    config.image_template = "/files/{path}";
    return compass::html::ops::render(source, compass::org::types::view(t),
        "doc/knowledge/page.org", config, corpus());
}

std::string body(std::string_view source) { return rendered(source).body; }

}

TEST_CASE("title and description are metadata, not body", "[html][render]") {
    const auto r = rendered(
        ":PROPERTIES:\n"
        ":ID: DOC-SELF\n"
        ":END:\n"
        "#+title: The Title\n"
        "#+description: What it is.\n"
        "#+type: knowledge\n"
        "#+filetags: :a:b:\n"
        "\n"
        "Text.\n");
    CHECK(r.id == "DOC-SELF");
    CHECK(r.title == "The Title");
    CHECK(r.description == "What it is.");
    CHECK_THAT(r.body, ContainsSubstring(
        "<dl class=\"org-keywords\">\n"
        "<dt>type</dt><dd>knowledge</dd>\n"
        "<dt>filetags</dt><dd>:a:b:</dd>\n"
        "</dl>\n"));
    CHECK_THAT(r.body, !ContainsSubstring("The Title"));
    CHECK_THAT(r.body, !ContainsSubstring("What it is."));
    CHECK_THAT(r.body, !ContainsSubstring("DOC-SELF"));
    CHECK_THAT(r.body, ContainsSubstring("<p>Text.\n</p>"));
    CHECK(r.unsupported.empty());
}

TEST_CASE("headings carry their anchor, TODO keyword, priority and tags",
    "[html][render]") {
    const auto b = body(
        "* TODO [#A] Fix it :work:urgent:\n"
        "SCHEDULED: <2026-11-22 Sun>\n"
        ":PROPERTIES:\n"
        ":ID: HEAD-SELF\n"
        ":END:\n"
        "** DONE Sub\n");
    CHECK_THAT(b, ContainsSubstring(
        "<section class=\"org-heading level-1\">\n"
        "<h2 id=\"HEAD-SELF\"><span class=\"org-todo\">TODO</span> "
        "<span class=\"org-priority\">[#A]</span> Fix it "
        "<span class=\"org-tags\"><span class=\"org-tag\">work</span>"
        "<span class=\"org-tag\">urgent</span></span></h2>\n"));
    CHECK_THAT(b, ContainsSubstring(
        "<span class=\"org-planning\">SCHEDULED: &lt;2026-11-22 Sun&gt;"
        "</span>"));
    CHECK_THAT(b, ContainsSubstring(
        "<h3 id=\"fix-it-sub\"><span class=\"org-todo org-done\">DONE"
        "</span> Sub</h3>"));
    CHECK_THAT(b, !ContainsSubstring(":PROPERTIES:"));
    CHECK_THAT(b, !ContainsSubstring(":ID:"));
}

TEST_CASE("drawers, comments and noexport headings are left out",
    "[html][render]") {
    const auto b = body(
        "Before.\n"
        ":LOGBOOK:\n"
        "- secret note\n"
        ":END:\n"
        "# a comment\n"
        "* Kept\n"
        "* Dropped :noexport:\n"
        "hidden text\n"
        "* COMMENT Also dropped\n"
        "hidden too\n");
    CHECK_THAT(b, ContainsSubstring("Before."));
    CHECK_THAT(b, ContainsSubstring("Kept"));
    CHECK_THAT(b, !ContainsSubstring("secret note"));
    CHECK_THAT(b, !ContainsSubstring("a comment"));
    CHECK_THAT(b, !ContainsSubstring("hidden"));
}

TEST_CASE("emphasis renders as inline markup", "[html][render]") {
    const auto b = body("*b* /i/ _u_ =v= ~c~ +s+ end\n");
    CHECK(b == "<p><strong>b</strong> <em>i</em> "
        "<span class=\"underline\">u</span> "
        "<code class=\"org-verbatim\">v</code> <code>c</code> "
        "<del>s</del> end\n</p>\n");
}

TEST_CASE("nested emphasis keeps its nesting", "[html][render]") {
    CHECK(body("*bold /and italic/*\n") ==
        "<p><strong>bold <em>and italic</em></strong>\n</p>\n");
}

TEST_CASE("plain, ordered and description lists", "[html][render]") {
    CHECK(body("- a\n- [X] b\n") ==
        "<ul>\n<li><p>a\n</p>\n</li>\n"
        "<li><span class=\"org-checkbox org-on\">[X]</span> <p>b\n</p>\n"
        "</li>\n</ul>\n");
    CHECK(body("1. one\n2. two\n") ==
        "<ol>\n<li><p>one\n</p>\n</li>\n<li><p>two\n</p>\n</li>\n</ol>\n");
    CHECK(body("- term :: definition\n") ==
        "<dl>\n<dt>term</dt><dd><p>definition\n</p>\n</dd>\n</dl>\n");
}

TEST_CASE("a table takes its header rows from the first rule",
    "[html][render]") {
    CHECK(body(
        "| Name | Value |\n"
        "|------+-------|\n"
        "| a    | =1=   |\n"
        "|------+-------|\n"
        "| b    | 2     |\n") ==
        "<table class=\"org-table\">\n"
        "<thead>\n<tr><th>Name</th><th>Value</th></tr>\n</thead>\n"
        "<tbody>\n"
        "<tr><td>a</td><td><code class=\"org-verbatim\">1</code></td></tr>\n"
        "<tr><td>b</td><td>2</td></tr>\n"
        "</tbody>\n</table>\n");
}

TEST_CASE("a table without a rule has no header", "[html][render]") {
    CHECK(body("| a | |\n") ==
        "<table class=\"org-table\">\n<tbody>\n"
        "<tr><td>a</td><td></td></tr>\n</tbody>\n</table>\n");
}

TEST_CASE("source and example blocks are escaped and keep their language",
    "[html][render]") {
    CHECK(body(
        "#+begin_src cpp\n"
        "if (a < b && c > d) {}\n"
        ",* not a heading\n"
        "#+end_src\n") ==
        "<pre class=\"org-src\"><code class=\"language-cpp\">"
        "if (a &lt; b &amp;&amp; c &gt; d) {}\n"
        "* not a heading\n"
        "</code></pre>\n");
    CHECK(body("#+begin_example\n<b>x</b>\n#+end_example\n") ==
        "<pre class=\"org-example\"><code>&lt;b&gt;x&lt;/b&gt;\n"
        "</code></pre>\n");
}

TEST_CASE("a source block language that is not a plain name is left out",
    "[html][render]") {
    CHECK(body("#+begin_src x\"onload=\n1\n#+end_src\n") ==
        "<pre class=\"org-src\"><code>1\n</code></pre>\n");
}

TEST_CASE("a quote block holds its paragraphs", "[html][render]") {
    CHECK(body("#+begin_quote\nSaid *so*.\n#+end_quote\n") ==
        "<blockquote>\n<p>Said <strong>so</strong>.\n</p>\n"
        "</blockquote>\n");
}

TEST_CASE("an id link to a document goes through the link template",
    "[html][render]") {
    CHECK(body("See [[id:DOC-B][B]].\n") ==
        "<p>See <a href=\"/docs/DOC-B\">B</a>.\n</p>\n");
}

TEST_CASE("an id link to a heading carries the heading's anchor",
    "[html][render]") {
    CHECK(body("See [[id:HEAD-A1][that part]].\n") ==
        "<p>See <a href=\"/docs/DOC-A#HEAD-A1\">that part</a>.\n</p>\n");
}

TEST_CASE("an id link to a heading of the same document is a fragment",
    "[html][render]") {
    const auto b = body(
        "* Target\n"
        ":PROPERTIES:\n"
        ":ID: HEAD-SELF\n"
        ":END:\n"
        "* Other\n"
        "Back to [[id:HEAD-SELF][target]] and [[*Target][again]].\n");
    CHECK_THAT(b, ContainsSubstring(
        "Back to <a href=\"#HEAD-SELF\">target</a> and "
        "<a href=\"#HEAD-SELF\">again</a>."));
}

TEST_CASE("a link to a document the caller cannot see is plain text",
    "[html][render]") {
    CHECK(body("See [[id:DOC-HIDDEN][the secret]].\n") ==
        "<p>See the secret.\n</p>\n");
    CHECK(body("See [[file:../secret.org][the secret]].\n") ==
        "<p>See the secret.\n</p>\n");
}

TEST_CASE("a link to an unknown document reads the same as a hidden one",
    "[html][render]") {
    CHECK(body("See [[id:NOWHERE][the secret]].\n") ==
        body("See [[id:DOC-HIDDEN][the secret]].\n"));
}

TEST_CASE("a file link to a known document goes through the link template",
    "[html][render]") {
    CHECK(body("See [[file:../other/b.org][B]].\n") ==
        "<p>See <a href=\"/docs/DOC-B\">B</a>.\n</p>\n");
    CHECK(body("See [[file:missing.org][M]].\n") == "<p>See M.\n</p>\n");
}

TEST_CASE("a file link to an image becomes an img", "[html][render]") {
    CHECK(body("[[file:images/a b.png]]\n") ==
        "<p><img src=\"/files/doc/knowledge/images/a%20b.png\" "
        "alt=\"images/a b.png\">\n</p>\n");
    CHECK(body("[[./../../x.svg]]\n") ==
        "<p><img src=\"/files/x.svg\" alt=\"./../../x.svg\">\n</p>\n");
}

TEST_CASE("a file link that leaves the root is plain text", "[html][render]") {
    CHECK(body("[[file:../../../etc/passwd.png]]\n") ==
        "<p>file:../../../etc/passwd.png\n</p>\n");
    CHECK(body("[[file:/etc/hosts][hosts]]\n") == "<p>hosts\n</p>\n");
}

TEST_CASE("http and https links stay as written", "[html][render]") {
    CHECK(body("[[https://example.com/a?b=1&c=2][site]] and "
        "http://example.org\n") ==
        "<p><a href=\"https://example.com/a?b=1&amp;c=2\">site</a> and "
        "<a href=\"http://example.org\">http://example.org</a>\n</p>\n");
}

TEST_CASE("other link types render as their text", "[html][render]") {
    CHECK(body("[[javascript:alert(1)][click]] [[mailto:a@b.c][mail]]\n") ==
        "<p>click mail\n</p>\n");
}

TEST_CASE("a script in text stays inert", "[html][render]") {
    CHECK(body("Hi <script>alert('x')</script> & bye\n") ==
        "<p>Hi &lt;script&gt;alert(&#39;x&#39;)&lt;/script&gt; &amp; bye\n"
        "</p>\n");
}

TEST_CASE("a script in a link description or target stays inert",
    "[html][render]") {
    CHECK(body("[[id:DOC-B][<script>x()</script>]]\n") ==
        "<p><a href=\"/docs/DOC-B\">&lt;script&gt;x()&lt;/script&gt;</a>\n"
        "</p>\n");
    CHECK(body("[[https://e.com/\"><script>x()</script>][go]]\n") ==
        "<p><a href=\"https://e.com/&quot;&gt;&lt;script&gt;x()&lt;/script"
        "&gt;\">go</a>\n</p>\n");
    CHECK(body("[[id:<script>][s]]\n") == "<p>s\n</p>\n");
}

TEST_CASE("export blocks and snippets are escaped, never passed through",
    "[html][render]") {
    const auto r = rendered(
        "#+begin_export html\n<script>x()</script>\n#+end_export\n"
        "\n"
        "#+attr_html: :onclick evil()\n"
        "A @@html:<b>bold</b>@@ snippet.\n");
    CHECK_THAT(r.body, ContainsSubstring(
        "<pre class=\"org-export\"><code>&lt;script&gt;x()&lt;/script&gt;\n"
        "</code></pre>\n"));
    CHECK_THAT(r.body, ContainsSubstring(
        "<span class=\"org-unsupported\" data-org-kind=\"export-snippet\">"
        "@@html:&lt;b&gt;bold&lt;/b&gt;@@</span>"));
    CHECK_THAT(r.body, !ContainsSubstring("<script>"));
    CHECK_THAT(r.body, !ContainsSubstring("<b>"));
    CHECK_THAT(r.body, !ContainsSubstring("onclick"));
}

TEST_CASE("an unsupported element is kept as escaped text",
    "[html][render]") {
    const auto r = rendered("[fn:1] A <note>.\n");
    CHECK(r.body ==
        "<div class=\"org-unsupported\" data-org-kind=\"footnote-definition\">"
        "<pre>[fn:1] A &lt;note&gt;.</pre></div>\n");
    CHECK(r.unsupported == std::vector<std::string>{"footnote-definition"});
}

TEST_CASE("timestamps and statistics cookies render as classed spans",
    "[html][render]") {
    CHECK(body("Due <2026-11-22 Sun> [1/2]\n") ==
        "<p>Due <span class=\"org-timestamp\">&lt;2026-11-22 Sun&gt;</span> "
        "<span class=\"org-statistics-cookie\">[1/2]</span>\n</p>\n");
}
