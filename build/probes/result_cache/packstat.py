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
"""Tally the entries of compass pack files, read-only: by kind, by hour
written, and how many copies of one kind and key the packs hold."""
import collections, datetime, mmap, pathlib, struct, sys

KINDS = {0: "object", 1: "manifest", 2: "result"}

def rows(path):
    with open(path, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as m:
        footer = len(m) - 32
        assert m[footer + 24:footer + 32] == b"CMPSEND\n", path
        index, count = struct.unpack_from("<QQ", m, footer)
        base = index + 256 * 4
        for i in range(count):
            r = base + i * 48
            key = bytes(m[r:r + 20])
            kind = m[r + 20]
            offset, size, written = struct.unpack_from("<QQq", m, r + 24)
            yield key, kind, size, written

def main(directory):
    packs = sorted(pathlib.Path(directory).glob("pack-*.cpk"))
    by_kind = collections.defaultdict(lambda: [0, 0])
    copies = collections.defaultdict(list)
    by_hour = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0]))
    sizes = collections.defaultdict(list)
    for p in packs:
        n = b = 0
        for key, kind, size, written in rows(p):
            k = KINDS.get(kind, str(kind))
            by_kind[k][0] += 1; by_kind[k][1] += size
            copies[(kind, key)].append((written, size, p.name))
            hour = datetime.datetime.fromtimestamp(written / 1e9, datetime.UTC).strftime("%m-%d %H:00")
            by_hour[hour][k][0] += 1; by_hour[hour][k][1] += size
            sizes[k].append(size)
            n += 1; b += size
        print(f"{p.name}: {n} entries, {b/1e9:.2f} GB")
    print("\nby kind: entries, GB, distinct keys, mean KB, max MB")
    distinct = collections.Counter(k for k, _ in copies)
    for kind, (n, b) in sorted(by_kind.items()):
        kid = [i for i, v in KINDS.items() if v == kind][0]
        s = sizes[kind]
        print(f"  {kind:9} {n:9} {b/1e9:7.2f} {distinct[kid]:9} {b/n/1e3:8.1f} {max(s)/1e6:8.2f}")
    dup = [(k, v) for k, v in copies.items() if len(v) > 1]
    dup_bytes = sum(sum(s for _, s, _ in sorted(v)[:-1]) for _, v in dup)
    print(f"\nkeys held more than once: {len(dup)}; bytes of the older copies: {dup_bytes/1e9:.2f} GB")
    print("\nwritten per hour (UTC): kind entries GB")
    for hour in sorted(by_hour):
        line = "  ".join(f"{k} {n} {b/1e9:.2f}" for k, (n, b) in sorted(by_hour[hour].items()))
        print(f"  {hour}  {line}")

main(sys.argv[1])
