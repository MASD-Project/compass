#!/usr/bin/env python3
#
# Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
"""Writes the archetype pages of masd.cpp.codec from compass.db's copy.

usage: codec_runtime_family.py <checkout> <ids.tsv>

The copy of ORE Studio's codec runtime was first adapted in place, under
projects/compass.db, and compiled there. This turns each file of it into an
archetype page of share/masd/templates whose template is the file with the
namespace, the include root and the include guard made parameters, so a
render for compass.db gives back the file. ids.tsv holds one archetype name
and its :ID: per line, made with uuidgen.
"""
import pathlib
import textwrap
import sys

ORE_COMMIT = "bc0702ad46b8a6097ce5242ece7ba01067699f60"
FACET_ID = "7D0FA277-D3F7-48E9-BCDF-7A88592119E3"

# name, rendered file, ORE Studio file, what it holds, how it differs
FILES = [
    ("wire_traits_fwd", "include/compass.db/codec/wire_traits_fwd.hpp",
     "include/ores.utility/codec/wire_traits_fwd.hpp",
     "The declaration of =wire_traits=, the seam a type's generated codec "
     "specialises.",
     "Verbatim but for the parameters."),
    ("wire_fields", "include/compass.db/codec/wire_fields.hpp",
     "include/ores.utility/codec/wire_fields.hpp",
     "=field=, =field_list=, =apply_fields=, =states_wire_form= and "
     "=for_each_field=: the statement of a type's shape.",
     "Verbatim but for the parameters."),
    ("member_count", "include/compass.db/codec/member_count.hpp",
     "include/ores.utility/codec/member_count.hpp",
     "=member_count= and =names_every_member=, which a round-trip test "
     "asserts so a member left out of a shape does not compile.",
     "Verbatim but for the parameters."),
    ("text_form", "include/compass.db/codec/text_form.hpp",
     "include/ores.utility/codec/text_form.hpp",
     "=text_form=, the text a type takes on the wire, and its forms for a "
     "string, a path and an instant.",
     "No =magic_enum= form for every enumeration: each enumeration's form is "
     "generated beside its type and states =names=. The tenant ID, date, "
     "time of day, decimal, address and uuid forms are left out: compass has "
     "none of those types. The instant is a "
     "=std::chrono::sys_time<std::chrono::microseconds>=, the C++ type of a "
     "=timestamptz= column, where ORE Studio's is a "
     "=system_clock::time_point=. No export macro."),
    ("text_form_impl", "src/codec/text_form.cpp",
     "src/codec/text_form.cpp",
     "The definitions of the forms =text_form.hpp= declares.",
     "The instant is formatted with =std::format= and parsed by hand, where "
     "ORE Studio calls its =datetime=, because libc++ has no "
     "=std::chrono::parse=. A path is written in its generic form, so the "
     "text is the same on Windows."),
    ("json_document", "include/compass.db/codec/json_document.hpp",
     "include/ores.utility/codec/json_document.hpp",
     "=json_document= and =json_value=: a JSON tree over yyjson, for a "
     "document with no fixed shape and for the schema.",
     "Verbatim but for the parameters."),
    ("json_codec", "include/compass.db/codec/json_codec.hpp",
     "include/ores.utility/codec/json_codec.hpp",
     "=json_codec=, the JSON carrier: an object over its fields, a number, "
     "a truth value, an optional, an array and a map, and =json::read= and "
     "=json::write=.",
     "The reader refuses a member the shape does not state, naming it, and "
     "names the member a refusal of its value is about. The error code and "
     "duration carriers are left out: compass has neither type. =<cstdlib>= "
     "is included for =free=."),
    ("json_schema", "include/compass.db/codec/json_schema.hpp",
     "include/ores.utility/codec/json_schema.hpp",
     "=json_schema<T>()=, the JSON Schema a type's shape implies.",
     "An object states =additionalProperties: false=, as its reader refuses "
     "an unknown member. An enumeration lists the =names= of its generated "
     "text form instead of =magic_enum='s. A type's name drops the product's "
     "namespace, a parameter. MSVC's =__FUNCSIG__= is read where clang and "
     "gcc's =__PRETTY_FUNCTION__= is."),
    ("struct_codec", "include/compass.db/codec/struct_codec.hpp",
     "include/ores.utility/codec/struct_codec.hpp",
     "=object::write_json= and =object::read_json=, a whole document from a "
     "type's shape, which a generated codec's definitions call.",
     "The MessagePack functions are left out."),
    ("sample", "include/compass.db/codec/sample.hpp",
     "include/ores.utility/codec/sample.hpp",
     "=sample<T>(n)=, values unlike the default and unlike each other, which "
     "a round-trip test writes and reads back.",
     "Enumerations sample the =values= of their text form. The samples of "
     "the types compass does not have are left out, and the instant's is a "
     "microsecond =sys_time=."),
    ("schema_check", "include/compass.db/codec/schema_check.hpp", None,
     "=schema_violation=, which checks a document against a schema "
     "=json_schema= wrote, for the test that a writer's output is a "
     "document its schema allows.",
     "compass's own: ORE Studio checks no schema."),
]

PARAMETERS = [
    ("compass::db::codec", "<%codec_namespace%>"),
    ("compass.db/codec/", "<%codec_root%>/"),
    ("COMPASS_DB_CODEC_", "<%codec_guard_prefix%>"),
    ('name.starts_with("compass::")', 'name.starts_with("<%product%>::")'),
    ('std::string("compass::").size()', 'std::string("<%product%>::").size()'),
    ("The leading =compass= is dropped", "The leading =<%product%>= is dropped"),
]

LICENCE_LINES = 19


def fill(text):
    return textwrap.fill(text, 76, break_long_words=False,
                         break_on_hyphens=False)


def template_of(text):
    body = "".join(text.splitlines(keepends=True)[LICENCE_LINES:])
    for old, new in PARAMETERS:
        body = body.replace(old, new)
    if "<%" in body.replace("<%codec", "").replace("<%product", ""):
        raise SystemExit("a file already holds the delimiter <%")
    return body


def page(name, ident, rendered, ore, holds, differs, text):
    stem = f"masd.cpp.codec.{name}"
    tangle = f"masd_cpp_codec_{name}.{'cpp' if rendered.endswith('.cpp') else 'hpp'}.mustache"
    output = (f"projects/{{component_dir}}/{rendered.replace('compass.db/', '{component_dir}/')}")
    origin = (f"ORE Studio's =projects/ores.utility/{ore}= at commit =={ORE_COMMIT}==."
              if ore else "No ORE Studio file: compass's own.")
    origin = origin.replace("==", "=")
    return f""":PROPERTIES:
:ID: {ident}
:END:
#+title: {stem}
#+description: {holds.replace('=', '')} Output {output}.
#+type: archetype
#+output: {output}
#+facet: masd.cpp.codec
#+level: cross
#+filetags: :codegen:templates:literate:archetype:masd:
#+created: 2026-11-23
#+updated: 2026-11-23

This page is an archetype of the [[id:{FACET_ID}][masd.cpp.codec]] facet: one
file of the JSON codec runtime, rendered into the component that switches
the facet on.

* Origin

{fill(origin)}

{fill(differs)}

* Context

The context is the library's values and =masd_component=: =codec_namespace=,
=codec_root=, the directory the runtime's headers are included from, and
=codec_guard_prefix=. The template switches the delimiters to =<% %>=,
because the C++ it holds has braces of its own.

* Template

The template source. compass's =tangle= codec writes it to
={tangle}= beside this page.

#+begin_src mustache :tangle {tangle}
{{{{! GENERATED FILE — tangled from share/masd/templates/{stem}.org. Edit the org source. }}}}
{{{{=<% %>=}}}}
<%& cpp_license%>
<%#masd_component%>
{text}<%/masd_component%>
#+end_src

* See also

- Parent facet: [[id:{FACET_ID}][masd.cpp.codec]]
"""


def main():
    root = pathlib.Path(sys.argv[1])
    ids = dict(line.split("\t") for line in
               pathlib.Path(sys.argv[2]).read_text().split("\n") if line)
    out = root / "share/masd/templates"
    for name, rendered, ore, holds, differs in FILES:
        text = (root / "projects/compass.db" / rendered).read_text()
        (out / f"masd.cpp.codec.{name}.org").write_text(
            page(name, ids[name], rendered, ore, holds, differs,
                 template_of(text)))


main()
