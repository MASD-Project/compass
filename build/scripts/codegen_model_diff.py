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
r"""Compare two dumps of the codegen models, model by model.

The expected dump comes from build/scripts/codegen_model_dump.py, which loads
the models through ores.codegen. The actual dump comes from
`compass-bench models <checkout> --dump <dir>`. Both write canonical JSON, one
file per model at <dir>/<model path>.json.

For every model of the expected dump the script reports whether the actual
dump is identical, by model type. For a model that differs it finds the
first difference in a walk of both values with keys in sorted order, and
counts it under its JSON path, with list positions written as []. It then
prints, for the models with the most differing leaves, the path and both
values of the first difference.

Usage: python3 build/scripts/codegen_model_diff.py <expected> <actual>
           [--worst N] [--only SUBSTRING]
"""
import argparse
import collections
import json
import os
import sys

MISSING = object()


def dumps(root, only):
    found = []
    for directory, subdirs, names in os.walk(root):
        subdirs.sort()
        for name in sorted(names):
            if name.endswith(".json"):
                relative = os.path.relpath(os.path.join(directory, name), root)
                if only is None or only in relative:
                    found.append(relative)
    return found


def load(path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return MISSING


def differences(expected, actual, path, out):
    """Appends (path, expected, actual) for every differing leaf, in order."""
    if isinstance(expected, dict) and isinstance(actual, dict):
        for key in sorted(set(expected) | set(actual)):
            differences(expected.get(key, MISSING), actual.get(key, MISSING),
                        f"{path}.{key}", out)
        return
    if isinstance(expected, list) and isinstance(actual, list):
        for i in range(max(len(expected), len(actual))):
            differences(expected[i] if i < len(expected) else MISSING,
                        actual[i] if i < len(actual) else MISSING,
                        f"{path}[]", out)
        return
    if type(expected) is not type(actual) or expected != actual:
        out.append((path, expected, actual))


def shown(value, width=100):
    if value is MISSING:
        return "<missing>"
    text = json.dumps(value, sort_keys=True, ensure_ascii=False)
    return text if len(text) <= width else text[:width] + "..."


def type_of(model):
    if isinstance(model, dict) and len(model) == 1:
        return next(iter(model))
    return "unknown"


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("expected")
    parser.add_argument("actual")
    parser.add_argument("--worst", type=int, default=20)
    parser.add_argument("--only")
    args = parser.parse_args()

    totals = collections.Counter()
    identical = collections.Counter()
    first_paths = collections.Counter()
    differing = []
    for relative in dumps(args.expected, args.only):
        expected = load(os.path.join(args.expected, relative))
        actual = load(os.path.join(args.actual, relative))
        kind = type_of(expected)
        totals[kind] += 1
        found = []
        differences(expected, actual, "", found)
        if not found:
            identical[kind] += 1
            continue
        first_paths[found[0][0]] += 1
        differing.append((len(found), relative, found[0]))

    total = sum(totals.values())
    same = sum(identical.values())
    print(f"identical: {same} of {total} models")
    for kind, count in sorted(totals.items(), key=lambda kv: -kv[1]):
        print(f"  {identical[kind]:5d} of {count:5d} {kind}")
    if first_paths:
        print("\nfirst difference by path:")
        for path, count in first_paths.most_common():
            print(f"  {count:5d} {path}")
    if differing and args.worst:
        print(f"\nfirst difference of the {args.worst} models with the most:")
        differing.sort(key=lambda d: (-d[0], d[1]))
        for count, relative, (path, expected, actual) in differing[:args.worst]:
            print(f"  {relative} ({count} differing leaves)")
            print(f"    {path}")
            print(f"      expected {shown(expected)}")
            print(f"      actual   {shown(actual)}")
    return 0 if same == total else 1


if __name__ == "__main__":
    sys.exit(main())
