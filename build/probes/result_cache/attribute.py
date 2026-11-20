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
"""Attribute the result objects of the packs in argv[2] to codecs: the
live ones by the keys the dependency records name (argv[1], the output of
recordkeys), the rest of 100 KB or more by the opening words they share
with a live result of a codec. Read-only."""
import sys, pathlib, mmap, struct, re, collections
units, evidence = sys.argv[1], sys.argv[2]
codec_of = {}
unit_kind = {}
for line in open(units):
    wt, codec, doc, res, man = line.rstrip("\n").split("\t")
    codec_of[bytes.fromhex(res)] = codec
    unit_kind[codec] = "document" if doc else "corpus"

def sig(head):
    w = re.findall(rb"[A-Za-z_][A-Za-z0-9_./-]{5,}", head)[:3]
    return b" ".join(w)

live = collections.defaultdict(lambda: [0, 0])
dead_by_sig = collections.defaultdict(lambda: [0, 0])
sig_codec = {}
dead_unknown = [0, 0]
entries = []
for p in sorted(pathlib.Path(evidence).glob("pack-*.cpk")):
    with open(p, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as m:
        footer = len(m) - 32; index, count = struct.unpack_from("<QQ", m, footer); base = index + 1024
        for i in range(count):
            r = base + i * 48
            if m[r + 20] != 2: continue
            key = bytes(m[r:r + 20]); off, size, w = struct.unpack_from("<QQq", m, r + 24)
            s = sig(bytes(m[off:off + 4096])) if size >= 100_000 else b"small"
            c = codec_of.get(key)
            if c:
                live[c][0] += 1; live[c][1] += size
                if size >= 100_000: sig_codec.setdefault(s, c)
            entries.append((c, s, size))
per = collections.defaultdict(lambda: [0, 0, 0, 0])  # live n, live b, dead n, dead b
for c, s, size in entries:
    if c:
        per[c][0] += 1; per[c][1] += size
    else:
        g = sig_codec.get(s, "(small)" if s == b"small" else "(unmatched large)")
        per[g][2] += 1; per[g][3] += size
print(f"{'codec':32} {'unit':8} {'live n':>8} {'live GB':>8} {'dead n':>8} {'dead GB':>8}")
for c, (ln, lb, dn, db) in sorted(per.items(), key=lambda x: -(x[1][1] + x[1][3]))[:25]:
    print(f"{c:32} {unit_kind.get(c,''):8} {ln:8} {lb/1e9:8.3f} {dn:8} {db/1e9:8.2f}")
