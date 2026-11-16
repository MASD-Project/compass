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
r"""Compare compass's contexts and raw renders with ores.codegen's.

<oracle> is a dump of build/scripts/codegen_render_dump.py; <compass> is
what `compass-bench render <checkout> --out <compass>` wrote. Both hold
renders/<output path>. Every output of the oracle is counted: identical,
different, or missing from compass. Outputs compass wrote that the oracle
has not are counted as extra, unless --template narrows the comparison.

When <compass> holds context_index.json, the contexts are compared first:
each output's context as compass built it against the one the oracle
rendered from, as JSON values. The report gives identical contexts of the
outputs compass built, by model type, and per JSON path, with list indices
written [*], how many outputs first differ there and an example. A first
difference is the first path, in sorted key order, where the two differ:
a value, a key only one side has, or a list length. With --every-path an
output counts under every path where it differs, which shows the progress
of a path that sorts after another difference. A Python float in the
oracle equals compass's string of the same text, because compass reads
real numbers as text.

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


def differences(expected, actual, path=""):
    """Every path where two JSON values differ, in sorted key order, with
    both sides. A list's items share the path [*]."""
    if isinstance(expected, float) and isinstance(actual, str):
        if repr(expected) != actual:
            yield path, expected, actual
        return
    if type(expected) is not type(actual):
        yield path, expected, actual
        return
    if isinstance(expected, dict):
        for key in sorted(set(expected) | set(actual)):
            at = f"{path}.{key}"
            if key not in actual:
                yield at, expected[key], "<absent>"
            elif key not in expected:
                yield at, "<absent>", actual[key]
            else:
                yield from differences(expected[key], actual[key], at)
        return
    if isinstance(expected, list):
        for e, a in zip(expected, actual):
            yield from differences(e, a, f"{path}[*]")
        if len(expected) != len(actual):
            yield f"{path}.length", len(expected), len(actual)
        return
    if expected != actual:
        yield path, expected, actual


def compare_contexts(oracle, compass, index, every_path):
    """The context differential, for the outputs compass built a context
    for. With every_path, each output counts once under every path where
    its contexts differ, not only under the first."""
    with open(os.path.join(compass, "context_index.json"),
              encoding="utf-8") as f:
        mine = {e["output"]: e for e in json.load(f)}
    by_type = collections.defaultdict(lambda: [0, 0])
    counted = collections.Counter()
    example = {}
    oracle_cache = {}
    compass_cache = {}
    verdicts = {}

    def load(cache, path):
        if path not in cache:
            if len(cache) > 64:
                cache.clear()
            with open(path, encoding="utf-8") as f:
                cache[path] = json.load(f)
        return cache[path]

    def verdict(expected, actual):
        found = {}
        for path, want, got in differences(expected, actual):
            found.setdefault(path, (want, got))
            if not every_path:
                break
        return found

    for entry in index:
        m = mine.get(entry["output"])
        if m is None:
            continue
        key = (entry["context"], m["context"])
        if key not in verdicts:
            verdicts[key] = verdict(
                load(oracle_cache, os.path.join(
                    oracle, "contexts", entry["context"] + ".json")),
                load(compass_cache, os.path.join(
                    compass, "contexts", m["context"] + ".json")))
        found = verdicts[key]
        counts = by_type[m["type"]]
        counts[1] += 1
        if not found:
            counts[0] += 1
        for path, (want, got) in found.items():
            counted[path] += 1
            example.setdefault(path, (entry["output"], want, got))

    same = sum(c[0] for c in by_type.values())
    total = sum(c[1] for c in by_type.values())
    print(f"contexts identical: {same} of {total} outputs "
          f"compass built a context for")
    for model_type, (same, of) in sorted(by_type.items()):
        print(f"{same:6} of {of:6} {model_type}")
    print("every difference by path:" if every_path
          else "first differences by path:")
    for path, count in counted.most_common():
        output, want, got = example[path]
        print(f"{count:6} {path}")
        print(f"    {output}")
        print(f"      oracle:  {json.dumps(want)[:160]}")
        print(f"      compass: {json.dumps(got)[:160]}")


def read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("oracle")
    parser.add_argument("compass")
    parser.add_argument("--every-path", action="store_true",
                        help="count every differing context path, not only "
                             "the first")
    parser.add_argument("--template",
                        help="only outputs of this template")
    args = parser.parse_args()

    with open(os.path.join(args.oracle, "index.json"), encoding="utf-8") as f:
        index = json.load(f)["outputs_by_path"]
    if args.template:
        index = [e for e in index if e["template"] == args.template]

    if os.path.exists(os.path.join(args.compass, "context_index.json")):
        compare_contexts(args.oracle, args.compass, index,
                         args.every_path)
        print()

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
