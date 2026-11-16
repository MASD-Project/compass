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
r"""Compare what compass tangled with the files Emacs tangled, target by target.

<checkout> holds the org sources and the committed targets Emacs wrote;
<out> is the output root of `compass generate --out <out>` run over it with
the tangle codec enabled. <dir>, relative to both, narrows the comparison
to one directory, such as projects/ores.codegen/library/templates.

Every org file under <dir> is a source. A target is a file compass wrote
under <out>/<dir>. Each is identical to the committed file of that path,
different, or new when the checkout has no such file. The report gives the
sources and how many have a block with a :tangle path, identical targets of
total, the first difference of each target that differs, and each committed
file under <dir> that is neither a source nor a target: a file no source
tangles.

Usage: python3 -I build/scripts/tangle_diff.py <checkout> <out> [<dir>]
"""
import argparse
import os
import re

TANGLED = re.compile(r"^[ \t]*#\+begin_src[ \t]+\S+.*[ \t]:tangle[ \t]+(\S+)",
                     re.IGNORECASE | re.MULTILINE)


def read(path):
    with open(path, "rb") as f:
        return f.read()


def files_under(root):
    for directory, _, names in os.walk(root):
        for name in names:
            yield os.path.relpath(os.path.join(directory, name), root)


def first_difference(expected, actual):
    """The line number and the two lines where the bytes first differ."""
    limit = min(len(expected), len(actual))
    at = next((i for i in range(limit) if expected[i] != actual[i]), limit)
    line = expected.count(b"\n", 0, at) + 1
    start = expected.rfind(b"\n", 0, at) + 1

    def line_of(text):
        end = text.find(b"\n", start)
        return text[start:end if end != -1 else len(text)]

    return line, line_of(expected), line_of(actual)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("checkout")
    parser.add_argument("out")
    parser.add_argument("dir", nargs="?", default="")
    args = parser.parse_args()

    source_root = os.path.join(args.checkout, args.dir)
    target_root = os.path.join(args.out, args.dir)
    sources = sorted(p for p in files_under(source_root) if p.endswith(".org"))
    tangling = [p for p in sources
                if any(m.group(1) != "no" for m in TANGLED.finditer(
                    read(os.path.join(source_root, p)).decode("utf-8")))]
    targets = sorted(files_under(target_root)) if os.path.isdir(
        target_root) else []

    identical = 0
    new = []
    differences = []
    for target in targets:
        committed = os.path.join(source_root, target)
        if not os.path.exists(committed):
            new.append(target)
            continue
        expected = read(committed)
        actual = read(os.path.join(target_root, target))
        if expected == actual:
            identical += 1
        else:
            differences.append((target,) + first_difference(expected, actual))

    produced = set(targets) | set(sources)
    untangled = sorted(p for p in files_under(source_root)
                       if p not in produced)

    print(f"sources: {len(sources)}; with a :tangle path: {len(tangling)}")
    print(f"identical: {identical} of {len(targets)} targets; "
          f"different {len(differences)}; new {len(new)}")
    for target, line, want, got in differences:
        print(f"  {target}:{line}")
        print(f"    emacs:   {want[:160]!r}")
        print(f"    compass: {got[:160]!r}")
    for target in new:
        print(f"  new: {target}")
    print(f"committed files no source tangles: {len(untangled)}")
    for path in untangled:
        print(f"  {path}")


if __name__ == "__main__":
    main()
