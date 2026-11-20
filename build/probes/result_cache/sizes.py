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
"""Bucket the result objects of the packs in argv[2] by size; argv[1] is
packstat.py, whose reader it uses. Read-only."""
import sys, pathlib, collections

src = pathlib.Path(sys.argv[1]).read_text().replace("main(sys.argv[1])", "")
ns = {}; exec(src, ns)
buckets = [(0, 1e4), (1e4, 1e5), (1e5, 1e6), (1e6, 1e7), (1e7, 1e9)]
t = collections.defaultdict(lambda: [0, 0])
for p in sorted(pathlib.Path(sys.argv[2]).glob("pack-*.cpk")):
    for key, kind, size, written in ns["rows"](p):
        if kind != 2: continue
        for lo, hi in buckets:
            if lo <= size < hi:
                t[(lo, hi)][0] += 1; t[(lo, hi)][1] += size
for (lo, hi), (n, b) in sorted(t.items()):
    print(f"results {lo/1e3:>8.0f} KB - {hi/1e3:>9.0f} KB: {n:8} entries {b/1e9:6.2f} GB")
