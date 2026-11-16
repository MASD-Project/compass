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
r"""Compare the compass org parser with org-element over a set of files.

For every .org file under a root, outside the top-level build/, external/
and vcpkg/ directories, the script takes two dumps in the same line format:
org-element's, from build/scripts/org_element_dump.el in batch Emacs, and
the parser's, from `compass-bench org-dump`. It reports how many files have
identical dumps and how many differ. For the files that differ it counts
the construct type of the first differing line, and prints that line for
the files with the most differing lines.

The oracle is slow, so its dumps can be kept between runs with --cache. A
cached dump is reused as long as it is newer than its source file and than
the dump script; delete the directory to force a full run. Emacs processes
are capped at four.

Usage: python3 build/scripts/org_diff.py <root> --bench <compass-bench>
           [--cache DIR] [--jobs N] [--worst N] [--only SUBSTRING]
"""
import argparse
import collections
import os
import subprocess
import sys
import tempfile

EXCLUDED_AT_ROOT = {"build", "external", "vcpkg", ".git"}
MAX_EMACS = 4
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DUMP_EL = os.path.join(SCRIPT_DIR, "org_element_dump.el")


def org_files(root, only):
    found = []
    for directory, subdirs, names in os.walk(root):
        if directory == root:
            subdirs[:] = [d for d in subdirs if d not in EXCLUDED_AT_ROOT]
        subdirs.sort()
        for name in sorted(names):
            if not name.endswith(".org"):
                continue
            relative = os.path.relpath(os.path.join(directory, name), root)
            if only is None or only in relative:
                found.append(relative)
    return found


def split_by_size(pairs, count):
    """Deal (input, output) pairs into count lists of similar total bytes."""
    lists = [[] for _ in range(count)]
    totals = [0] * count
    for pair in sorted(pairs, key=lambda p: -os.path.getsize(p[0])):
        i = totals.index(min(totals))
        lists[i].append(pair)
        totals[i] += os.path.getsize(pair[0])
    return [l for l in lists if l]


def run_lists(command, pairs, jobs, scratch, tag):
    """Run command over pairs, one process per list, and wait for all."""
    processes = []
    for i, chunk in enumerate(split_by_size(pairs, jobs)):
        path = os.path.join(scratch, f"{tag}.{i}.list")
        with open(path, "w", encoding="utf-8") as f:
            for source, output in chunk:
                f.write(f"{source}\t{output}\n")
        processes.append(subprocess.Popen(
            command + [path], stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE))
    failures = []
    for p in processes:
        _, err = p.communicate()
        if p.returncode != 0:
            failures.append(err.decode("utf-8", "replace").strip())
    return failures


def stale(output, *sources):
    if not os.path.exists(output):
        return True
    made = os.path.getmtime(output)
    return any(os.path.getmtime(s) > made for s in sources)


def make_dirs(paths):
    for directory in {os.path.dirname(p) for p in paths}:
        os.makedirs(directory, exist_ok=True)


def read_lines(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8", "replace").split("\n")


def construct_of(line):
    """The type an oracle or parser line starts with."""
    words = line.split()
    return words[0] if words else "(end of dump)"


def compare(oracle, parser):
    """First differing line pair and the count of differing lines."""
    first = None
    differing = abs(len(oracle) - len(parser))
    for i, (a, b) in enumerate(zip(oracle, parser)):
        if a != b:
            differing += 1
            if first is None:
                first = (i + 1, a, b)
    if first is None and len(oracle) != len(parser):
        i = min(len(oracle), len(parser))
        first = (i + 1, oracle[i] if i < len(oracle) else "",
                 parser[i] if i < len(parser) else "")
    return first, differing


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("root")
    ap.add_argument("--bench", required=True,
                    help="path of the compass-bench executable")
    ap.add_argument("--cache", help="directory that keeps the oracle dumps")
    ap.add_argument("--jobs", type=int, default=MAX_EMACS)
    ap.add_argument("--worst", type=int, default=20)
    ap.add_argument("--only", help="keep files whose path contains this")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    files = org_files(root, args.only)
    if not files:
        sys.exit(f"no .org files under {root}")

    with tempfile.TemporaryDirectory(prefix="org_diff.") as scratch:
        cache = os.path.abspath(args.cache) if args.cache else \
            os.path.join(scratch, "oracle")
        ours = os.path.join(scratch, "parser")
        sources = {f: os.path.join(root, f) for f in files}
        oracle_of = {f: os.path.join(cache, f + ".dump") for f in files}
        parser_of = {f: os.path.join(ours, f + ".dump") for f in files}
        make_dirs(list(oracle_of.values()) + list(parser_of.values()))

        wanted = [(sources[f], oracle_of[f]) for f in files
                  if stale(oracle_of[f], sources[f], DUMP_EL)]
        failures = run_lists(
            ["emacs", "--batch", "-l", DUMP_EL, "--", "--list"], wanted,
            max(1, min(args.jobs, MAX_EMACS)), scratch, "oracle")
        failures += run_lists(
            [os.path.abspath(args.bench), "org-dump", "--list"],
            [(sources[f], parser_of[f]) for f in files],
            max(1, args.jobs), scratch, "parser")
        for failure in failures:
            print(f"FAILED: {failure}", file=sys.stderr)

        identical = 0
        by_type = collections.Counter()
        differing = []
        for f in files:
            oracle = read_lines(oracle_of[f])
            parser = read_lines(parser_of[f])
            first, count = compare(oracle, parser)
            if first is None:
                identical += 1
                continue
            line, a, b = first
            by_type[construct_of(a if a.strip() else b)] += 1
            differing.append((count, f, line, a, b))

    print(f"files      {len(files)}")
    print(f"identical  {identical}")
    print(f"differing  {len(differing)}")
    print(f"oracle dumps made this run  {len(wanted)}")
    if by_type:
        print("\nfirst difference by construct")
        for name, n in by_type.most_common():
            print(f"{n:>8}  {name}")
    if differing:
        differing.sort(key=lambda d: (-d[0], d[1]))
        print(f"\nfirst differing line of the {args.worst} worst files")
        for count, f, line, a, b in differing[:args.worst]:
            print(f"{f}: {count} lines differ, first at line {line}")
            print(f"  org-element: {a.strip()}")
            print(f"  compass:     {b.strip()}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
