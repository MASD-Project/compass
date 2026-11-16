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
r"""Record where pystache renders a Mustache spec test differently.

compass's Mustache engine has pystache's semantics, because ores.codegen
renders with pystache, so the spec suite is the reference only where pystache
agrees with it. This renders every test of the vendored spec files with
pystache's default Renderer, HTML escaping included, and writes the tests
whose output differs from the spec's expected text, as
{"<file>/<test name>": "<pystache output>"}, which the engine's spec test
expects instead.

Run it with a Python that has pystache installed:

  python3 -I build/scripts/mustache_spec_deviations.py \
      projects/compass.codegen/tests/data/mustache_spec
"""
import json
import os
import sys

import pystache


def main():
    directory = sys.argv[1]
    deviations = {}
    for name in sorted(os.listdir(directory)):
        if not name.endswith(".json") or name == "pystache_deviations.json":
            continue
        with open(os.path.join(directory, name), encoding="utf-8") as f:
            spec = json.load(f)
        for test in spec["tests"]:
            renderer = pystache.Renderer(partials=test.get("partials", {}))
            actual = renderer.render(test["template"], test["data"])
            if actual != test["expected"]:
                deviations[name[:-5] + "/" + test["name"]] = actual
    target = os.path.join(directory, "pystache_deviations.json")
    with open(target, "w", encoding="utf-8") as f:
        f.write(json.dumps(deviations, indent=2, sort_keys=True,
                           ensure_ascii=False) + "\n")
    print(f"{len(deviations)} deviations written to {target}")


if __name__ == "__main__":
    main()
