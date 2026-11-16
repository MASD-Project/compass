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
r"""Compare compass's raw renders with ores.codegen's, output by output.

<oracle> is a dump of build/scripts/codegen_render_dump.py; <compass> is
what `compass-bench render <checkout> --out <compass>` wrote. Both hold
renders/<output path>. Every output of the oracle is counted: identical,
different, or missing from compass. Outputs compass wrote that the oracle
has not are counted as extra, unless --template narrows the comparison.

The report gives identical outputs of total and the time the oracle's
render_template calls took for the outputs compass rendered, the baseline
for compass-bench render's time. Then one line per template, in order of
how many outputs it has: identical of total, missing, and the first
difference of its first differing output, with the line it is on.

Usage: python3 -I build/scripts/codegen_render_diff.py <oracle> <compass>
           [--template NAME]
"""
import argparse
import collections
import json
import os


def first_difference(expected, actual):
    """The line number and the two lines where the texts first differ."""
    limit = min(len(expected), len(actual))
    at = next((i for i in range(limit) if expected[i] != actual[i]), limit)
    line = expected.count("\n", 0, at) + 1
    start = expected.rfind("\n", 0, at) + 1

    def line_of(text):
        end = text.find("\n", start)
        return text[start:end if end != -1 else len(text)]

    return line, line_of(expected), line_of(actual)


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("oracle")
    parser.add_argument("compass")
    parser.add_argument("--template",
                        help="only outputs of this template")
    args = parser.parse_args()

    with open(os.path.join(args.oracle, "index.json"), encoding="utf-8") as f:
        index = json.load(f)["outputs_by_path"]
    if args.template:
        index = [e for e in index if e["template"] == args.template]

    stats = collections.defaultdict(lambda: {"total": 0, "identical": 0,
                                             "missing": 0, "first": None})
    identical = 0
    oracle_seconds = 0.0
    for entry in index:
        s = stats[entry["template"]]
        s["total"] += 1
        mine = os.path.join(args.compass, "renders", entry["output"])
        if not os.path.exists(mine):
            s["missing"] += 1
            continue
        oracle_seconds += entry.get("seconds", 0.0)
        expected = read(os.path.join(args.oracle, "renders", entry["output"]))
        actual = read(mine)
        if expected == actual:
            s["identical"] += 1
            identical += 1
        elif s["first"] is None:
            s["first"] = (entry["output"],) + first_difference(expected,
                                                               actual)

    known = {e["output"] for e in index}
    extra = 0
    renders = os.path.join(args.compass, "renders")
    for directory, _, names in os.walk(renders):
        for name in names if not args.template else ():
            path = os.path.relpath(os.path.join(directory, name), renders)
            if path not in known:
                extra += 1

    missing = sum(s["missing"] for s in stats.values())
    print(f"identical: {identical} of {len(index)} outputs; "
          f"missing {missing}; different "
          f"{len(index) - identical - missing}; extra {extra}")
    print(f"oracle render_template time of the outputs compass rendered: "
          f"{oracle_seconds * 1000:.1f} ms")
    for template, s in sorted(stats.items(),
                              key=lambda kv: (-kv[1]["total"], kv[0])):
        print(f"{s['identical']:6} of {s['total']:6} {template}"
              + (f" (missing {s['missing']})" if s["missing"] else ""))
        if s["first"]:
            output, line, want, got = s["first"]
            print(f"    {output}:{line}")
            print(f"      oracle:  {want[:160]!r}")
            print(f"      compass: {got[:160]!r}")


if __name__ == "__main__":
    main()
