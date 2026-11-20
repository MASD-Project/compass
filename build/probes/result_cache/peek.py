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
"""Group the result objects of 100 KB or more of the packs in argv[2] by
the opening words of their bodies; argv[1] is packstat.py. Read-only."""
import sys, pathlib, mmap, struct, re, collections
src = pathlib.Path(sys.argv[1]).read_text().replace("main(sys.argv[1])", ""); ns = {}; exec(src, ns)
big = []
for p in sorted(pathlib.Path(sys.argv[2]).glob("pack-*.cpk")):
    with open(p, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as m:
        footer = len(m) - 32; index, count = struct.unpack_from("<QQ", m, footer); base = index + 1024
        for i in range(count):
            r = base + i * 48
            kind = m[r + 20]; off, size, w = struct.unpack_from("<QQq", m, r + 24)
            if kind == 2 and size >= 100_000:
                head = bytes(m[off:off + 4096])
                words = re.findall(rb"[A-Za-z_][A-Za-z0-9_./-]{5,}", head)[:12]
                big.append((size, p.name[5:13], b" ".join(words).decode(errors="replace")[:200]))
sig = collections.Counter()
sigb = collections.Counter()
for size, pk, s in big:
    k = " ".join(s.split()[:3]); sig[k] += 1; sigb[k] += size
for k, n in sigb.most_common(15):
    print(f"{n/1e9:6.2f} GB {sig[k]:6} results  {k[:150]}")
