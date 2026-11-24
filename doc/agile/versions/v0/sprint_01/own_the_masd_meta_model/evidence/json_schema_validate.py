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
"""usage: json_schema_validate.py <probe output>

Checks the probe's output with the jsonschema package: every schema is valid 2020-12,
allows its documents, refuses an added member and each required member removed,
and allows each other member removed. A body schema allows the document cut to
its properties and refuses the whole document when that states more."""
import json
import sys

from jsonschema import Draft202012Validator, FormatChecker

failures = 0
checked = 0


def expect(ok, what):
    global failures, checked
    checked += 1
    if not ok:
        failures += 1
        print("FAIL", what)


def check(schema, docs, name):
    Draft202012Validator.check_schema(schema)
    expect(schema.get("type") == "object", f"{name}: root type object")
    v = Draft202012Validator(schema, format_checker=FormatChecker())
    for i, doc in enumerate(docs):
        errors = list(v.iter_errors(doc))
        expect(not errors, f"{name} doc {i}: {[e.message for e in errors][:2]}")
    doc = docs[0]
    expect(not v.is_valid({**doc, "unknown_member_added_by_a_test": True}),
           f"{name}: added member allowed")
    required = schema.get("required", [])
    for member in doc:
        cut = {k: val for k, val in doc.items() if k != member}
        if member in required:
            expect(not v.is_valid(cut), f"{name}: {member} removed allowed")
        else:
            expect(v.is_valid(cut), f"{name}: {member} removed refused")


for line in open(sys.argv[1]):
    entry = json.loads(line)
    check(entry["schema"], entry["docs"], entry["type"])
    if "body_schema" in entry:
        body = entry["body_schema"]
        names = set(body["properties"])
        docs = [{k: val for k, val in d.items() if k in names} for d in entry["docs"]]
        check(body, docs, entry["type"] + " body")
        full = entry["docs"][0]
        if set(full) - names:
            expect(not Draft202012Validator(body).is_valid(full),
                   entry["type"] + " body: whole document allowed")

print(f"{checked} checks, {failures} failed")
sys.exit(1 if failures else 0)
