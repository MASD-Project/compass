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
"""Find where MSVC's C1202 fires on ORE Studio's column lists.

Each probe is one translation unit of synthetic entities, each with as many
members as its width and a columns() chain built as ORE Studio's entity
headers build theirs, with ORE Studio's columns.hpp (a copy, taken at
e866b76f3e). A probe is a list of widths, in the order the unit defines the
entities. The probes are:

- unit53 and unit55: the entity widths ores.trading.core's unity units 53
  and 55 include, in include order, at e866b76f3e. Unit 55 failed with
  C1202 on its last entity; unit 53 compiled.
- single_<w>: one entity of width w.
- many_<k>x<w>: k entities of width w.

Each is compiled with cl.exe and the flags ORE Studio's release build
passes, and its outcome is ok, C1202 or another error. The results go to
results.tsv in the output directory, and to the step summary on CI.

    python build/probes/msvc_c1202/probe.py --out <dir> [--only <name>...]
"""

from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import time

HERE = pathlib.Path(__file__).resolve().parent
EVIDENCE = (HERE.parents[2] / "doc/agile/versions/v0/sprint_01/"
            "keep_msvc_within_its_template_limits/evidence")
FLAGS = ["/nologo", "/c", "/TP", "/std:c++latest", "/constexpr:depth100000",
         "/constexpr:steps10000000", "/O2", "/Ob2", "/EHsc", "/MD", "/Z7",
         "/bigobj", "/W4", "/DNDEBUG"]
MEMBER_TYPES = ["int", "std::string", "std::optional<std::string>",
                "std::optional<int>", "double", "std::string"]


def widths_of(unit: int) -> list[int]:
    path = EVIDENCE / f"trading_core_unit_{unit}_entities_e866b76f3e.tsv"
    rows = path.read_text().splitlines()[1:]
    return [int(r.split("\t")[1]) for r in rows if r]


def probes() -> dict[str, list[int]]:
    out = {"unit53": widths_of(53), "unit55": widths_of(55)}
    for w in (20, 75, 150, 300, 600):
        out[f"single_{w}"] = [w]
    for k, w in ((60, 20), (120, 20), (240, 20), (480, 20), (60, 40),
                 (120, 40), (30, 75), (60, 75)):
        out[f"many_{k}x{w}"] = [w] * k
    return out


def entity(n: int, width: int) -> str:
    members = "\n".join(
        f"    {MEMBER_TYPES[m % len(MEMBER_TYPES)]} m{m};"
        for m in range(width))
    chain = " |\n            ".join(
        [f"qb::columns<e>()", 'qb::key("m0", &e::m0)'] +
        [f'qb::bind("m{m}", &e::m{m})' for m in range(1, width)])
    return (f"struct entity_{n} {{\n{members}\n"
            f"    static constexpr auto columns() {{\n"
            f"        using e = entity_{n};\n"
            f"        return {chain};\n    }}\n}};\n")


def source(widths: list[int]) -> str:
    body = "\n".join(entity(n, w) for n, w in enumerate(widths))
    return ("#include <optional>\n#include <string>\n"
            '#include "columns.hpp"\n\n'
            "namespace qb = ores::database::query_builder;\n\n"
            f"namespace probe {{\n\n{body}\n}}\n")


def compile_one(path: pathlib.Path) -> tuple[str, float, str]:
    start = time.monotonic()
    r = subprocess.run(["cl.exe", *FLAGS, f"/I{HERE}", str(path),
                        f"/Fo{path.with_suffix('.obj')}"],
                       capture_output=True, text=True, cwd=path.parent)
    seconds = time.monotonic() - start
    text = r.stdout + r.stderr
    if r.returncode == 0:
        return "ok", seconds, ""
    first = next((l for l in text.splitlines() if "error" in l), "")
    return ("C1202" if "C1202" in text else "error"), seconds, first.strip()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, type=pathlib.Path)
    parser.add_argument("--only", nargs="*")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    rows = ["probe\tentities\tbindings\twidest\toutcome\tseconds\tfirst_error"]
    for name, widths in probes().items():
        if args.only and name not in args.only:
            continue
        path = args.out / f"{name}.cpp"
        path.write_text(source(widths))
        outcome, seconds, first = compile_one(path)
        rows.append(f"{name}\t{len(widths)}\t{sum(widths)}\t{max(widths)}"
                    f"\t{outcome}\t{seconds:.1f}\t{first}")
        print(rows[-1], flush=True)
    (args.out / "results.tsv").write_text("\n".join(rows) + "\n")
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a") as f:
            header, *body = rows
            cols = header.split("\t")
            f.write("| " + " | ".join(cols) + " |\n")
            f.write("|" + "---|" * len(cols) + "\n")
            for r in body:
                f.write("| " + " | ".join(r.split("\t")) + " |\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
