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
r"""Dump every ORE Studio codegen model as ores.codegen loads it.

This is the oracle for the compass model loader. Run it with a Python that
has ores.codegen's requirements installed, pointed at an ORE Studio checkout
or archive. It only reads the checkout.

The entry point is codegen.core.load_model(path). Generation reaches a model
through it and nothing else: generate.resolve_targets() calls it to gate the
facets, and core.generate_from_model() calls it before it renders. It reads
the #+type: keyword and dispatches to the org_loader function of that type.
Everything after it in generate_from_model() (licences, template data, the
*_file references) is render context, not the model.

A model is an .org file in a modeling/ directory under projects/ whose
#+type: names an ores.codegen type that load_model knows. Files of other
ores.codegen types (module, the component and modeline catalogues) are
counted as unloadable and skipped.

The dump is canonical JSON, one file per model at <out>/<model path>.json:
keys sorted, two-space indent, sets as sorted lists, floats in their
shortest round-trip form, which is what Python's repr writes; an infinity or
a NaN is written as the string repr gives it. A model whose load raises is
written as {"error": "<exception type>"}.

Usage: python3 build/scripts/codegen_model_dump.py <checkout> <out>
           [--only SUBSTRING] [--repeat N]
"""
import argparse
import collections
import json
import math
import os
import re
import sys
import time

TYPE_RE = re.compile(r"^#\+type:\s*(\S+)\s*$", re.MULTILINE | re.IGNORECASE)


def model_files(root, only):
    projects = os.path.join(root, "projects")
    found = []
    for directory, subdirs, names in os.walk(projects):
        subdirs.sort()
        if os.path.basename(directory) != "modeling":
            continue
        for name in sorted(names):
            if not name.endswith(".org"):
                continue
            relative = os.path.relpath(os.path.join(directory, name), root)
            if only is None or only in relative:
                found.append(relative)
    return found


def declared_type(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        head = f.read(4096)
    match = TYPE_RE.search(head)
    return match.group(1) if match else None


def canonical(value):
    if isinstance(value, dict):
        return {str(k): canonical(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [canonical(v) for v in value]
    if isinstance(value, (set, frozenset)):
        return sorted(canonical(v) for v in value)
    if isinstance(value, float):
        if not math.isfinite(value):
            return repr(value)
        return value
    if isinstance(value, (str, int, bool)) or value is None:
        return value
    return str(value)


def write_dump(out, relative, model):
    target = os.path.join(out, relative + ".json")
    os.makedirs(os.path.dirname(target), exist_ok=True)
    text = json.dumps(canonical(model), sort_keys=True, indent=2,
                      ensure_ascii=False)
    with open(target, "w", encoding="utf-8") as f:
        f.write(text + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("checkout")
    parser.add_argument("out")
    parser.add_argument("--only")
    parser.add_argument("--repeat", type=int, default=1,
                        help="load the set this many times and report each")
    args = parser.parse_args()

    root = os.path.abspath(args.checkout)
    sys.path.insert(0, os.path.join(root, "projects", "ores.codegen", "src"))
    from codegen import core

    known = core._ORG_TYPE_TO_MODEL_TYPE
    models = []
    unloadable = collections.Counter()
    for relative in model_files(root, args.only):
        kind = declared_type(os.path.join(root, relative))
        if kind is None or not kind.startswith("ores.codegen."):
            continue
        if kind not in known:
            unloadable[kind] += 1
            continue
        models.append((relative, kind))

    by_type = collections.Counter(kind for _, kind in models)
    errors = collections.Counter()
    for run in range(args.repeat):
        loaded = []
        start = time.perf_counter()
        for relative, kind in models:
            try:
                loaded.append((relative,
                               core.load_model(os.path.join(root, relative))))
            except Exception as e:
                loaded.append((relative, {"error": type(e).__name__}))
                if run == 0:
                    errors[kind] += 1
        elapsed = time.perf_counter() - start
        print(f"run {run + 1}: loaded {len(models)} models in {elapsed:.3f} s")

    for relative, model in loaded:
        write_dump(args.out, relative, model)

    for kind, count in sorted(by_type.items(), key=lambda kv: -kv[1]):
        failed = errors.get(kind, 0)
        print(f"{count:6d} {kind}" + (f" ({failed} raised)" if failed else ""))
    for kind, count in sorted(unloadable.items()):
        print(f"{count:6d} {kind} (not a load_model type, skipped)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
