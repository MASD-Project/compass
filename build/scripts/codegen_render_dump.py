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
r"""Capture every raw render of ores.codegen and the context it came from.

This is the oracle for the compass renderer. Run it with a Python that has
ores.codegen's requirements installed, pointed at an ORE Studio checkout or
archive. It writes nothing into the checkout.

It runs generate.cmd_regenerate with --all, which is what
`codegen.sh regenerate --all` runs, with three interceptions and nothing
reimplemented:

- generate.clang_format_files and generate.prettier_format_files become
  no-ops, so what is captured is the text before formatting.
- core.render_template is wrapped: the context dict it is called with is
  dumped before the call, and the time of the call itself is summed. That
  dict is the whole input of the render; render_template adds the generated
  markers to the licences and cpp_value_init to the fields on its own copy.
- core's open() is replaced for writes: generate_from_model writes the
  rendered text, after the <<paste:UUID>> substitution, with
  open(output_path, 'w'); that text is captured under the output's path
  relative to the checkout instead of written.

Each output is written to <out>/renders/<path>. Its context is written once
per distinct content, to <out>/contexts/<sha1>.json, as compact JSON with
sorted keys: sets as sorted lists, paths and other objects as str(), floats
as JSON numbers. The outputs of one model mostly share a context, so this
keeps the dump to about a tenth of one file per output.
<out>/index.json lists every output with its template, its model, its
context's sha1 and whether the paste substitution changed the mustache
output, and the seconds its render_template call took. A run writes some
outputs more than once, because more than one component regenerates their
models; an output keeps its last write, as the checkout would.
render_seconds is the time of every render_template call;
output_render_seconds counts only the last render of each output, the set
a renderer that writes each output once is timed on, and
codegen_render_diff.py sums the seconds of any subset.

Usage: python3 -I build/scripts/codegen_render_dump.py <checkout> <out>
"""
import argparse
import builtins
import contextlib
import hashlib
import io
import json
import logging
import os
import sys
import time
import types


def canonical(value):
    if isinstance(value, dict):
        return {str(k): canonical(v) for k, v in value.items()}
    if isinstance(value, (list, tuple)):
        return [canonical(v) for v in value]
    if isinstance(value, (set, frozenset)):
        return sorted((canonical(v) for v in value), key=repr)
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    return str(value)


class Capture(io.StringIO):
    def __init__(self, on_close):
        super().__init__()
        self._on_close = on_close

    def close(self):
        self._on_close(self.getvalue())
        super().close()


def write_text(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("checkout")
    parser.add_argument("out")
    args = parser.parse_args()

    root = os.path.realpath(args.checkout)
    out = os.path.abspath(args.out)
    base_dir = os.path.join(root, "projects", "ores.codegen")
    sys.path.insert(0, os.path.join(base_dir, "src"))
    os.chdir(root)
    logging.basicConfig(level=logging.WARNING)

    from pathlib import Path
    from codegen import core, generate

    generate.clang_format_files = lambda paths: None
    generate.prettier_format_files = lambda paths, base: None

    state = {"model": None, "template": None, "context": None,
             "mustache": None, "render_seconds": 0.0, "renders": 0}
    index = {}

    original_render = core.render_template

    def render_template(template_path, data):
        state["template"] = os.path.basename(str(template_path))
        state["context"] = json.dumps(canonical(data), sort_keys=True,
                                      separators=(",", ":"),
                                      ensure_ascii=False)
        start = time.perf_counter()
        rendered = original_render(template_path, data)
        state["seconds"] = time.perf_counter() - start
        state["render_seconds"] += state["seconds"]
        state["renders"] += 1
        state["mustache"] = rendered
        return rendered

    core.render_template = render_template

    original_generate = generate.generate_from_model

    def generate_from_model(model_path, *rest, **kwargs):
        state["model"] = os.path.relpath(os.path.realpath(model_path), root)
        return original_generate(model_path, *rest, **kwargs)

    generate.generate_from_model = generate_from_model

    def captured(output_path, text):
        relative = os.path.relpath(os.path.realpath(output_path), root)
        write_text(os.path.join(out, "renders", relative), text)
        digest = hashlib.sha1(state["context"].encode("utf-8")).hexdigest()
        context_path = os.path.join(out, "contexts", digest + ".json")
        if not os.path.exists(context_path):
            write_text(context_path, state["context"] + "\n")
        index[relative] = {"output": relative,
                           "template": state["template"],
                           "model": state["model"], "context": digest,
                           "pasted": text != state["mustache"],
                           "seconds": state["seconds"]}

    def core_open(file, mode="r", *rest, **kwargs):
        if "w" in mode:
            return Capture(lambda text: captured(file, text))
        return builtins.open(file, mode, *rest, **kwargs)

    core.open = core_open

    command = types.SimpleNamespace(all=True, component=None, address=None,
                                    dry_run=False, entity=None)
    start = time.perf_counter()
    with contextlib.redirect_stdout(io.StringIO()):
        rc = generate.cmd_regenerate(command, Path(base_dir))
    elapsed = time.perf_counter() - start

    outputs = [index[k] for k in sorted(index)]
    output_seconds = sum(e["seconds"] for e in outputs)
    summary = {"exit_code": rc, "outputs": len(outputs),
               "renders": state["renders"],
               "generate_seconds": round(elapsed, 3),
               "render_seconds": round(state["render_seconds"], 3),
               "output_render_seconds": round(output_seconds, 3),
               "outputs_by_path": outputs}
    write_text(os.path.join(out, "index.json"),
               json.dumps(summary, indent=1, sort_keys=True) + "\n")
    print(f"exit {rc}: {len(outputs)} outputs from {state['renders']} renders; "
          f"generation {elapsed:.3f} s, render_template "
          f"{state['render_seconds']:.3f} s, of which the last render of "
          f"each output {output_seconds:.3f} s")


if __name__ == "__main__":
    main()
