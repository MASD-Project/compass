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
"""Writes compass.db's codec runtime back into the masd.cpp.codec pages.

usage: codec_runtime_sync.py <checkout>

A change to the runtime is made and compiled in compass.db's rendered copy.
This puts each rendered file back into the template block of its archetype
page, with the namespace, the include root and the include guard made
parameters again, so compass generate renders the same file. Run on an
unchanged copy it changes no page.
"""

import pathlib
import sys

from codec_runtime_family import FILES, PARAMETERS

BANNER_END = " * To modify, update the template and regenerate.\n */\n"
OPEN = "<%#masd_component%>\n"
CLOSE = "<%/masd_component%>\n#+end_src"


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    root = pathlib.Path(sys.argv[1])
    files = [(f[0], f[1]) for f in FILES]
    files.append(("base64", "include/compass.db/codec/base64.hpp"))
    for name, rendered in files:
        text = (root / "projects/compass.db" / rendered).read_text()
        body = text[text.index(BANNER_END) + len(BANNER_END):]
        for old, new in PARAMETERS:
            body = body.replace(old, new)
        path = root / f"share/masd/templates/masd.cpp.codec.{name}.org"
        page = path.read_text()
        start = page.index(OPEN) + len(OPEN)
        end = page.index(CLOSE)
        updated = page[:start] + body + page[end:]
        if updated != page:
            path.write_text(updated)
            print(f"{path.relative_to(root)}: updated")


if __name__ == "__main__":
    main()
