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
"""The column bindings each unity unit of an ORE Studio target includes.

CMake fills a target's unity units with its sources in list order, 16 to a
unit. For each unit this follows the quoted and angled includes that resolve
to a project header, and counts the query_builder bindings of every entity
header the unit includes: how many, their sum, the widest, and the sum of
their squares. Given a header name it then reports, for each unit that
includes it, where in the unit's include order it comes and how many
bindings the unit has included before it.

    python3 build/scripts/unity_unit_bindings.py <ore checkout> \
        <target source dir> <list of the target's sources> [<header> | unit=<n>]
"""

import re, sys, pathlib, functools
root = pathlib.Path(sys.argv[1])
src_dir = root / sys.argv[2]
files = [l.strip() for l in open(sys.argv[3]) if l.strip()]
batch = 16
INC = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)
index = {}
for inc in root.glob("projects/*/include"):
    for p in inc.rglob("*.hpp"):
        index[str(p.relative_to(inc))] = p
for inc in root.glob("projects/*/*/include"):
    for p in inc.rglob("*.hpp"):
        index[str(p.relative_to(inc))] = p
@functools.lru_cache(None)
def direct(p):
    out = []
    for name in INC.findall(p.read_text(errors="ignore")):
        if name in index:
            out.append(index[name])
    return tuple(out)
def closure(start):
    seen, stack = set(), list(start)
    while stack:
        p = stack.pop()
        if p in seen: continue
        seen.add(p)
        stack.extend(direct(p))
    return seen
BIND = re.compile(r'query_builder::(?:bind|key|bind_text|key_text)\(')
@functools.lru_cache(None)
def bindings(p):
    return len(BIND.findall(p.read_text(errors="ignore")))
units = [files[i:i+batch] for i in range(0, len(files), batch)]
print("unit\theaders\tentities\tbindings\tmax_entity\tsum_sq")
for n, u in enumerate(units):
    hs = closure([src_dir / f for f in u])
    ents = [h for h in hs if bindings(h) > 0]
    b = [bindings(h) for h in ents]
    print(f"{n}\t{len(hs)}\t{len(ents)}\t{sum(b)}\t{max(b) if b else 0}\t{sum(x*x for x in b)}")

def preorder(start):
    seen, order = set(), []
    def visit(p):
        if p in seen: return
        seen.add(p)
        for d in direct(p):
            visit(d)
        order.append(p)
    for s in start:
        visit(s)
    return order
target = sys.argv[4] if len(sys.argv) > 4 else None
if target and target.startswith("unit="):
    print("header\tbindings")
    for h in preorder([src_dir / f for f in units[int(target[5:])]]):
        if bindings(h) > 0:
            print(f"{h.name}\t{bindings(h)}")
elif target:
    print("unit\tposition\tbindings_before\tentities_before\tbindings_total")
    for n, u in enumerate(units):
        order = preorder([src_dir / f for f in u])
        names = [str(p).endswith(target) for p in order]
        if True in names:
            i = names.index(True)
            before = [bindings(p) for p in order[:i] if bindings(p) > 0]
            total = sum(bindings(p) for p in order)
            print(f"{n}\t{i}/{len(order)}\t{sum(before)}\t{len(before)}\t{total}")
