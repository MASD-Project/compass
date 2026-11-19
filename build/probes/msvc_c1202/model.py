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
"""Fit MSVC's C1202 rule for column-list chains, and apply it.

The rule: in one translation unit, count every operator| of every entity's
columns() chain, in the order the unit defines the entities. MSVC fails at
the first operator| that is at least the budget-th of the unit and whose
result holds at least the length-th binding.

    model.py fit <probe results.tsv>...
        Every (budget, length) that predicts every chain probe's outcome.
    model.py predict <unit entities.tsv> [budget length]
        The first failure of each unit of a table of unit, header and
        bindings rows, as unity_unit_bindings.py lists them.
"""

from __future__ import annotations

import collections
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import probe  # noqa: E402

BUDGET = 1026
LENGTH = 18
CHAINS = ("columns", "int_columns", "uniq_columns", "real")


def first_failure(widths, budget=BUDGET, length=LENGTH):
    """The (entity index, binding) MSVC fails at, or None."""
    applied = 0
    for e, width in enumerate(widths):
        for binding in range(1, width + 1):
            applied += 1
            if applied >= budget and binding >= length:
                return e, binding
    return None


def chain_widths(name, shape, entities):
    if shape != "real":
        return entities
    return probe.widths_of(55 if "unit55" in name else 53)[:len(entities)]


def fit(paths):
    probes = probe.probes()
    observed = {}
    for path in paths:
        for line in pathlib.Path(path).read_text().splitlines()[1:]:
            cells = line.split("\t")
            if len(cells) > 4 and cells[4] in ("ok", "C1202"):
                observed[cells[0]] = cells[4] == "C1202"
    cases = [(chain_widths(n, *probes[n]), failed)
             for n, failed in observed.items()
             if n in probes and probes[n][0] in CHAINS]
    fits = [(b, l) for b in range(900, 1200) for l in range(2, 40)
            if all((first_failure(w, b, l) is not None) == failed
                   for w, failed in cases)]
    print(f"{len(cases)} chain probes; fits: {fits}")
    return 0 if fits else 1


def predict(path, budget=BUDGET, length=LENGTH):
    units = collections.defaultdict(list)
    for line in pathlib.Path(path).read_text().splitlines()[1:]:
        unit, header, bindings = line.split("\t")
        units[int(unit)].append((header, int(bindings)))
    print("unit\theader\tbinding")
    for unit, rows in sorted(units.items()):
        hit = first_failure([b for _, b in rows], budget, length)
        if hit:
            print(f"{unit}\t{rows[hit[0]][0]}\t{hit[1]}")
    return 0


def main() -> int:
    command, *rest = sys.argv[1:]
    if command == "fit":
        return fit(rest)
    if command == "predict":
        numbers = [int(x) for x in rest[1:3]]
        return predict(rest[0], *numbers)
    raise SystemExit(__doc__)


if __name__ == "__main__":
    raise SystemExit(main())
