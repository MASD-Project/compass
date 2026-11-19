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
"""Find where MSVC's C1202 fires on ORE Studio's per-struct templates.

Each probe is one translation unit of synthetic structs, each with as many
members as its width, and a list of widths, in the order the unit defines
the structs. Two shapes are probed, each with a copy of ORE Studio's header:

- columns: an entity whose columns() chain appends one binding per member
  with operator|, as ORE Studio's entity headers do (columns.hpp, taken at
  e866b76f3e).
- fields: a type whose wire_fields() returns one field_list of a field per
  member, as ORE Studio's generated codecs do, walked once with
  for_each_field (wire_fields.hpp, taken at d9af8eeda6).

The probes are:

- unit53 and unit55: the entity widths ores.trading.core's unity units 53
  and 55 include, in include order, at e866b76f3e. Unit 55 failed with
  C1202 on its last entity; unit 53 compiled.
- <shape>_single_<w>: one struct of width w.
- <shape>_many_<k>x<w>: k structs of width w.
- sweep_<w>_<k>: k entities of width w, k stepped across the count at
  which the first run failed, to find the exact threshold per width.
- unit<u>_first<n>: unit u's first n entities, to find where it fails.
- int_many_<k>x<w>: k entities of width w whose members are all int, to
  see whether member types change the cost.
- real_unit<u>[_first<n>]: ORE Studio's own entity headers, as unit u
  includes them, at e866b76f3e (copies under ore/, with stand-ins for the
  four headers they include besides the column list).
- uniq_...: the synthetic probes with member names unique to each entity,
  as ORE Studio's are, where the others repeat m0, m1 and so on.
- rev_unit<u>_first<n>: unit u's entities in reverse order, first n, to
  see whether order changes where the unit fails.
- w75_first_<k> and w75_last_<k>: one entity of width 75 before, or after,
  k entities of width 20, to see whether a wide entity's cost depends on
  where it comes.
- w10x50_then_20_<k>: 50 entities of width 10, then k of width 20, to
  price narrow entities against the width-20 threshold.
- pin_length_<w>: 110 entities of width 10, 1,100 bindings with no list
  longer than 10, then one of width w: it fails when w reaches the
  shortest list length that can fail.
- pin_budget_<w>: 100 entities of width 10, then one of width w: it
  fails when 1,000 + w reaches the budget and w that length.
- flat_...: the column lists built in one call, make_columns
  (flat_columns.hpp), instead of a chain of operator|.

Each is compiled with cl.exe and the flags ORE Studio's release build
passes, and its outcome is ok, C1202 or another error. The results go to
results.tsv in the output directory, and to the step summary on CI. Another
error means the probe itself is broken, so the command then fails.

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
MIXED_TYPES = ["int", "std::string", "std::optional<std::string>",
               "std::optional<int>", "double", "std::string"]
MEMBER_TYPES = MIXED_TYPES
SWEEPS = {1: range(900, 1101, 25), 5: range(190, 216), 10: range(95, 111),
          20: range(45, 61), 40: range(22, 29), 75: range(11, 16)}


def headers_of(unit: int) -> list[str]:
    path = EVIDENCE / f"trading_core_unit_{unit}_entities_e866b76f3e.tsv"
    names = [r.split("\t")[0] for r in path.read_text().splitlines()[1:] if r]
    index = {h.name: h.relative_to(HERE / "ore").as_posix()
             for h in (HERE / "ore").rglob("*_entity.hpp")}
    return [index[n] for n in names]


def widths_of(unit: int) -> list[int]:
    path = EVIDENCE / f"trading_core_unit_{unit}_entities_e866b76f3e.tsv"
    rows = path.read_text().splitlines()[1:]
    return [int(r.split("\t")[1]) for r in rows if r]


def probes() -> dict[str, tuple[str, list[int]]]:
    out = {"unit53": ("columns", widths_of(53)),
           "unit55": ("columns", widths_of(55))}
    for w, ks in SWEEPS.items():
        for k in ks:
            out[f"sweep_{w}_{k}"] = ("columns", [w] * k)
    for u, ns in ((55, range(40, 58)), (53, range(30, 60))):
        widths = widths_of(u)
        for n in ns:
            out[f"unit{u}_first{n}"] = ("columns", widths[:n])
    for u, ns in ((55, range(50, 58)), (53, range(36, 60))):
        for n in ns:
            out[f"real_unit{u}_first{n}"] = ("real", headers_of(u)[:n])
    for u in (53, 55):
        widths = list(reversed(widths_of(u)))
        for n in range(30, len(widths) + 1):
            out[f"rev_unit{u}_first{n}"] = ("columns", widths[:n])
    for k in range(38, 53):
        out[f"w75_first_{k}"] = ("columns", [75] + [20] * k)
        out[f"w75_last_{k}"] = ("columns", [20] * k + [75])
    for k in range(20, 53, 2):
        out[f"w10x50_then_20_{k}"] = ("columns", [10] * 50 + [20] * k)
    for w in range(14, 25):
        out[f"pin_length_{w}"] = ("columns", [10] * 110 + [w])
    for w in range(18, 46):
        out[f"pin_budget_{w}"] = ("columns", [10] * 100 + [w])
    out["flat_unit53"] = ("flat_columns", widths_of(53))
    out["flat_unit55"] = ("flat_columns", widths_of(55))
    for k, w in ((120, 20), (480, 20), (60, 75), (240, 40)):
        out[f"flat_many_{k}x{w}"] = ("flat_columns", [w] * k)
    for k in range(45, 61):
        out[f"uniq_sweep_20_{k}"] = ("uniq_columns", [20] * k)
    out["uniq_unit53"] = ("uniq_columns", widths_of(53))
    out["uniq_unit55"] = ("uniq_columns", widths_of(55))
    out["int_many_60x20"] = ("int_columns", [20] * 60)
    out["int_many_45x20"] = ("int_columns", [20] * 45)
    for shape in ("columns", "fields"):
        for w in (20, 75, 150, 300):
            out[f"{shape}_single_{w}"] = (shape, [w])
        for k, w in ((60, 20), (120, 20), (240, 20), (480, 20), (60, 40),
                     (120, 40), (30, 75), (60, 75)):
            out[f"{shape}_many_{k}x{w}"] = (shape, [w] * k)
    return out


def members_of(width: int) -> str:
    return "\n".join(f"    {MEMBER_TYPES[m % len(MEMBER_TYPES)]} m{m};"
                     for m in range(width))


def flat_entity(n: int, width: int) -> str:
    bindings = ",\n            ".join(
        ['qb::key("m0", &e::m0)'] +
        [f'qb::bind("m{m}", &e::m{m})' for m in range(1, width)])
    return (f"struct entity_{n} {{\n{members_of(width)}\n"
            f"    static constexpr auto columns() {{\n"
            f"        using e = entity_{n};\n"
            f"        return qb::make_columns<e>(\n            {bindings});\n"
            f"    }}\n}};\n")


def uniq_entity(n: int, width: int) -> str:
    return entity(n, width).replace(" m", f" e{n}_m").replace(
        '"m', f'"e{n}_m').replace("::m", f"::e{n}_m")


def int_entity(n: int, width: int) -> str:
    global MEMBER_TYPES
    MEMBER_TYPES = ["int"]
    try:
        return entity(n, width)
    finally:
        MEMBER_TYPES = MIXED_TYPES


def fields_type(n: int, width: int) -> str:
    fields = ",\n        ".join(f'wc::field{{"m{m}", &type_{n}::m{m}}}'
                                 for m in range(width))
    return (f"struct type_{n} {{\n{members_of(width)}\n}};\n"
            f"constexpr auto wire_fields(std::type_identity<type_{n}>) {{\n"
            f"    return wc::field_list{{\n        {fields}}};\n}}\n"
            f"std::size_t count_{n}(const type_{n}& v) {{\n"
            f"    std::size_t n = 0;\n"
            f"    wc::for_each_field(v, [&](std::string_view, const auto&) "
            f"{{ ++n; }});\n    return n;\n}}\n")


def entity(n: int, width: int) -> str:
    members = members_of(width)
    chain = " |\n            ".join(
        [f"qb::columns<e>()", 'qb::key("m0", &e::m0)'] +
        [f'qb::bind("m{m}", &e::m{m})' for m in range(1, width)])
    return (f"struct entity_{n} {{\n{members}\n"
            f"    static constexpr auto columns() {{\n"
            f"        using e = entity_{n};\n"
            f"        return {chain};\n    }}\n}};\n")


def source(shape: str, widths: list) -> str:
    if shape == "real":
        return "".join(f'#include "{h}"\n' for h in widths)
    make = {"columns": entity, "int_columns": int_entity,
            "uniq_columns": uniq_entity, "flat_columns": flat_entity,
            "fields": fields_type}[shape]
    body = "\n".join(make(n, w) for n, w in enumerate(widths))
    return ("#include <cstddef>\n#include <optional>\n#include <string>\n"
            "#include <string_view>\n#include <type_traits>\n"
            '#include "flat_columns.hpp"\n#include "wire_fields.hpp"\n\n'
            "namespace qb = ores::database::query_builder;\n"
            "namespace wc = ores::utility::codec;\n\n"
            f"namespace probe {{\n\n{body}\n}}\n")


def compile_one(path: pathlib.Path) -> tuple[str, float, str]:
    start = time.monotonic()
    r = subprocess.run(["cl.exe", *FLAGS, f"/I{HERE}", f"/I{HERE / 'ore'}",
                        str(path),
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
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    rows = ["probe\tentities\tbindings\twidest\toutcome\tseconds\tfirst_error"]
    broken = 0
    for name, (shape, widths) in probes().items():
        if args.only and name not in args.only:
            continue
        path = args.out / f"{name}.cpp"
        path.write_text(source(shape, widths))
        outcome, seconds, first = compile_one(path)
        broken += outcome == "error"
        sizes = widths if shape != "real" else [0]
        rows.append(f"{name}\t{len(widths)}\t{sum(sizes)}\t{max(sizes)}"
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
    return 1 if broken else 0


if __name__ == "__main__":
    raise SystemExit(main())
