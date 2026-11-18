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
r"""Compare each ORE Studio generator with the compass codecs that replace it.

<export> is a git repository holding an ORE Studio tree, such as a `git
archive` of its main committed once; it is the checkout compass runs on, and
is left as it was found. <work> is a scratch directory: the generators run in
a clone of <export> under it, so their writes never touch <export>.

For each generator in GENERATORS:

1. `compass generate --out` runs on <export> with only the generator's codecs
   enabled, into a directory under <work>.
2. The clone is reset to <export>'s commit, the generator's commands run in it
   in write mode, and every file they wrote is listed: a file whose mtime is
   after the run started, outside .git and not ignored by git.
3. Every file compass emitted is compared with the clone's file of that path:
   what the generator wrote, or the committed file where it wrote nothing.
   Every file the generator wrote that compass did not emit is missing.
4. Every emitted file gets the same plants (plant_lines) in <export> and
   in the reset clone, and the generator runs again. `compass check` with the
   same codecs must report <codec>/drift on exactly the planted files the
   generator rewrote: a plant outside a generated region, which the
   generator keeps, is not drift.
5. A generator that leaves an up-to-date file alone writes nothing step 1 can
   see, so every tracked file its Python opened that compass did not emit is
   planted in the reset clone (UNPLANTED aside) and the generator runs once
   more: each planted file it rewrites is an output compass is missing.

The codegen codec has no generator to run: `compass check` with codegen
alone on <export> lists the committed files that differ from their models.
That drift, and drift on the committed outputs of a generator, is ORE
Studio's and is reported without failing the run.

One line per generator goes to standard output; the paths behind each count
go to <work>/<name>.txt. The exit status is 0 when every emitted file is
identical, none is missing, no generator command fails and compass reports
drift on exactly the plants the generator rewrites, else 1.

Usage: verify_generators.py --compass BIN --python PY --work DIR EXPORT [NAME...]
PY runs the Python generators, under -I and -B, from a venv holding
ores.codegen's requirements. NAME narrows the run to the named generators,
and `codegen` to the codegen comparison.
"""
import argparse
import dataclasses
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

# Runs one Python entry point as `python script args` would, with only the
# script's directory put back on the path -I removes, and appends every path
# it opens to the file its first argument names.
LAUNCH = (
    "import atexit, os, runpy, sys\n"
    "record, sys.argv = sys.argv[1], sys.argv[2:]\n"
    "opened = set()\n"
    "def hook(event, args):\n"
    "    if event == 'open' and isinstance(args[0], (str, bytes, os.PathLike)):\n"
    "        opened.add(os.path.abspath(os.fsdecode(args[0])))\n"
    "sys.addaudithook(hook)\n"
    "def save():\n"
    "    with open(record, 'a', encoding='utf-8', errors='surrogateescape') as f:\n"
    "        f.writelines(p + '\\n' for p in sorted(opened))\n"
    "atexit.register(save)\n"
    "sys.path.insert(0, os.path.dirname(os.path.abspath(sys.argv[0])))\n"
    "runpy.run_path(sys.argv[0], run_name='__main__')\n"
)

# Files the probe does not plant: a plant in a module or a structured input
# stops the generator rather than showing what it writes.
UNPLANTED = (".py", ".el", ".json", ".xml", ".xsd", ".tsv", ".csv", ".mustache")

EMACS = ("emacs", "-Q", "--script")
SQL = "projects/ores.sql"
CODEGEN = "projects/ores.codegen"
ER_MODEL = "build/output/codegen/plantuml_er_model.json"


@dataclasses.dataclass(frozen=True)
class Generator:
    """An ORE Studio generator: the commands that run it in write mode, and
    the compass codecs that replace it."""
    name: str
    codecs: tuple[str, ...]
    commands: tuple[tuple[str, ...], ...]


def py(*argv: str) -> tuple[str, ...]:
    return ("python",) + argv


GENERATORS = (
    Generator("tangle", ("tangle",),
              (EMACS + ("projects/ores.lisp/src/ores-build-codegen-templates.el",),)),
    Generator("shell_recipe_library", ("shell_recipe_library",),
              (EMACS + ("projects/ores.lisp/src/ores-build-recipe-scripts.el",),)),
    Generator("http_recipe_library", ("http_recipe_library",),
              (EMACS + ("projects/ores.lisp/src/ores-build-http-recipes.el",),)),
    Generator("component_files", ("component_files",),
              (py(f"{CODEGEN}/scripts/regenerate_cmake_component_files.py", "--all"),)),
    Generator("skill_catalogue", ("skill_catalogue",),
              (py("build/scripts/generate_skills_catalogue.py"),)),
    Generator("skill_type_tables", ("skill_type_tables",),
              (py("build/scripts/generate_skill_type_tables.py"),)),
    Generator("skill_levels", ("skill_levels",),
              (py("build/scripts/apply_skill_levels.py"),)),
    Generator("recipe_inventories", ("recipe_inventories",),
              (py(f"{CODEGEN}/scripts/regenerate_shell_recipe_inventory.py"),
               py(f"{CODEGEN}/scripts/regenerate_http_recipe_inventory.py"))),
    Generator("backlog_indexes", ("backlog_indexes",),
              (py(f"{CODEGEN}/scripts/regenerate_backlog_indexes.py"),)),
    Generator("physical_inventory", ("physical_inventory",),
              (py(f"{CODEGEN}/scripts/regenerate_physical_space_inventories.py"),)),
    Generator("pending_models", ("pending_models",),
              (py(f"{CODEGEN}/scripts/derive_pending_models.py"),)),
    Generator("trade_type_coverage", ("trade_type_coverage",),
              (py("build/scripts/generate_trade_type_coverage.py", "--write"),)),
    Generator("route_inventory", ("route_inventory",),
              (py("build/scripts/route_inventory.py", "--write"),)),
    Generator("ore_renderings_tables", ("ore_renderings_tables",),
              (py("projects/ores.ore/scripts/generate_renderings_tables.py"),)),
    Generator("script_library_seed", ("script_library_seed",),
              (py(f"{SQL}/utility/generate_script_library.py"),)),
    Generator("curve_sections_wiring", ("curve_sections_wiring",),
              (py(f"{SQL}/utility/generate_curve_section_parameters.py"),
               py(f"{SQL}/utility/generate_curve_column_guards.py"))),
    Generator("ore_output_catalogue", ("ore_output_catalogue",),
              (py("projects/ores.ore/scripts/generate_output_catalogue.py"),)),
    Generator("er_diagram", ("er_diagram",),
              (py(f"{CODEGEN}/src/plantuml_er_parse_sql.py",
                  "--create-dir", f"{SQL}/create", "--drop-dir", f"{SQL}/drop",
                  "--output", ER_MODEL,
                  "--ignore-file", f"{SQL}/utility/validation_ignore.txt"),
               py(f"{CODEGEN}/src/plantuml_er_generate.py", "--model", ER_MODEL,
                  "--template", f"{CODEGEN}/library/templates/plantuml_er.mustache",
                  "--index-template",
                  f"{CODEGEN}/library/templates/plantuml_er_index.mustache",
                  "--output", f"{SQL}/modeling/ores_schema.puml"))),
    Generator("conventions_dataset", ("conventions_dataset",),
              (py("projects/ores.seeder/datasets/conventions/generate_sql.py"),)),
    Generator("ore_value_sets", ("ore_value_sets",),
              (py(f"{SQL}/utility/generate_convention_kind_terms.py"),
               py(f"{SQL}/utility/generate_value_sets.py"),
               py(f"{SQL}/utility/generate_ore_element_map.py"))),
)

PLANT = b" compass-verify-plant"


@dataclasses.dataclass
class Comparison:
    written: list[str]
    emitted: list[str]
    identical: list[str]
    different: list[str]
    missing: list[str]
    planted: list[str]
    restored: list[str]
    caught: list[str]
    clean_drift: list[str]
    failures: list[str]
    probed: int


def git(repo: pathlib.Path, *args: str) -> str:
    return subprocess.run(("git", "-C", str(repo)) + args, check=True,
                          capture_output=True, text=True).stdout


def files_under(root: pathlib.Path) -> list[str]:
    if not root.is_dir():
        return []
    return sorted(str(p.relative_to(root)) for p in root.rglob("*") if p.is_file())


def written_since(repo: pathlib.Path, start: float) -> list[str]:
    """Files under repo modified at or after start, outside .git and not
    ignored by git."""
    found = []
    for dirpath, dirnames, filenames in os.walk(repo):
        if dirpath == str(repo) and ".git" in dirnames:
            dirnames.remove(".git")
        for f in filenames:
            p = pathlib.Path(dirpath, f)
            if not p.is_symlink() and p.stat().st_mtime >= start:
                found.append(str(p.relative_to(repo)))
    if not found:
        return []
    ignored = subprocess.run(
        ("git", "-C", str(repo), "check-ignore", "--no-index", "--stdin"),
        input="\n".join(found), capture_output=True, text=True).stdout.split("\n")
    return sorted(set(found) - set(ignored))


def reset(clone: pathlib.Path) -> None:
    git(clone, "reset", "-q", "--hard")
    git(clone, "clean", "-qfdx")


def run_generator(g: Generator, clone: pathlib.Path, python: str,
                  log) -> tuple[list[str], list[str], list[str]]:
    """Runs the generator in the clone as it stands; the files it wrote, the
    commands that failed, and the files under the clone its Python opened."""
    record = clone / ".git" / "verify_opened"
    record.unlink(missing_ok=True)
    time.sleep(0.05)
    start = time.time()
    time.sleep(0.05)
    failures = []
    for command in g.commands:
        argv = ((python, "-I", "-B", "-c", LAUNCH, str(record)) + command[1:]
                if command[0] == "python" else command)
        r = subprocess.run(argv, cwd=clone, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, errors="replace")
        log.write(f"$ {' '.join(command)}\nexit {r.returncode}\n{r.stdout}\n")
        if r.returncode != 0:
            script = command[1] if command[0] == "python" else command[-1]
            failures.append(f"{script} exit {r.returncode}")
    opened = set()
    if record.is_file():
        for line in record.read_text(encoding="utf-8", errors="surrogateescape").splitlines():
            p = pathlib.Path(line)
            if p.is_relative_to(clone):
                opened.add(str(p.relative_to(clone)))
    return written_since(clone, start), failures, sorted(opened)


def compass_args(codecs: tuple[str, ...], config: pathlib.Path) -> list[str]:
    """--set arguments that turn every codec of the configuration off but
    the named ones."""
    rows = re.findall(r"^\|\s*([a-z_]+)\s*\|\s*(?:yes|no)\s*\|", config.read_text(), re.M)
    sets = []
    for name in rows:
        if name not in codecs:
            sets += ["--set", f"{name}.enabled=false"]
    for name in codecs:
        sets += ["--set", f"{name}.enabled=true"]
    return sets


def drift_paths(output: str, codecs: tuple[str, ...]) -> list[str]:
    """The paths `compass check` reports under <codec>/drift."""
    rules = "|".join(re.escape(c) for c in codecs)
    pattern = re.compile(rf"^(.+?):\d+:\d+: (?:{rules})/drift\b", re.M)
    return sorted(set(pattern.findall(output)))


def compass(binary: str, export: pathlib.Path, args: list[str], log) -> str:
    r = subprocess.run([binary, "-C", str(export)] + args, capture_output=True,
                       text=True, errors="replace")
    log.write(f"$ compass {' '.join(args)}\nexit {r.returncode}\n{r.stderr}\n{r.stdout}\n")
    if r.returncode not in (0, 1):
        raise RuntimeError(f"compass {args[0]} exit {r.returncode}: {r.stderr.strip()}")
    return r.stdout


def plant_lines(lines: list[bytes]) -> list[bytes]:
    """The lines with the plant: PLANT appended to the middle line and to
    every table row, and the middle list item deleted. A page updated in
    place keeps its generated region in a table or a list, and a list may
    carry a hand-written note after each item, so an item is deleted rather
    than appended to."""
    rows = {i for i, line in enumerate(lines) if line.lstrip().startswith(b"|")}
    items = [i for i, line in enumerate(lines) if line.lstrip().startswith(b"- ")]
    appended = rows | {len(lines) // 2}
    deleted = items[len(items) // 2] if items else None
    return [line + PLANT if i in appended else line
            for i, line in enumerate(lines) if i != deleted]


def plant(root: pathlib.Path, paths: list[str]) -> list[str]:
    """Plants each file of paths that root holds with plant_lines, the same
    edit in every tree; the files planted."""
    planted = []
    for p in paths:
        f = root / p
        if f.is_file():
            f.write_bytes(b"\n".join(plant_lines(f.read_bytes().split(b"\n"))))
            planted.append(p)
    return planted


def compare(g: Generator, export: pathlib.Path, work: pathlib.Path,
            clone: pathlib.Path, binary: str, python: str, log) -> Comparison:
    """Compares the generator with its codecs on the tree as committed, then
    with one plant in each output in both trees: compass must report drift on
    the planted files the generator rewrites, and only on those."""
    out = work / "out" / g.name
    shutil.rmtree(out, ignore_errors=True)
    sets = compass_args(g.codecs, export / ".compass.org")
    compass(binary, export, ["generate", "--out", str(out)] + sets, log)
    emitted = files_under(out)
    reset(clone)
    written, failures, opened = run_generator(g, clone, python, log)
    identical, different = [], []
    for p in emitted:
        mine = clone / p
        same = mine.is_file() and mine.read_bytes() == (out / p).read_bytes()
        (identical if same else different).append(p)
    missing = sorted(set(written) - set(emitted))
    clean = drift_paths(compass(binary, export, ["check"] + sets, log), g.codecs)

    reset(clone)
    planted = plant(clone, emitted)
    kept = {p: (clone / p).read_bytes() for p in planted}
    rewritten, more, _ = run_generator(g, clone, python, log)
    restored = sorted(p for p in planted
                      if not (clone / p).is_file() or (clone / p).read_bytes() != kept[p])
    missing = sorted(set(missing) | (set(rewritten) - set(emitted)))
    written = sorted(set(written) | set(rewritten))
    plant(export, emitted)
    try:
        caught = drift_paths(compass(binary, export, ["check"] + sets, log), g.codecs)
    finally:
        if planted:
            git(export, "checkout", "-q", "--", *planted)
    caught = sorted(set(caught) & set(planted))

    # A generator that leaves an up-to-date file alone writes nothing the
    # listing can see, but it opened the file to compare. Planting every
    # tracked file it opened that compass does not emit makes it rewrite
    # each one it owns.
    reset(clone)
    tracked = set(git(clone, "ls-files", "-z").split("\0"))
    suspects = [p for p in opened if p in tracked and p not in emitted
                and not p.endswith(UNPLANTED)]
    probed = plant(clone, suspects)
    kept = {p: (clone / p).read_bytes() for p in probed}
    _, probe_failures, _ = run_generator(g, clone, python, log)
    owned = sorted(p for p in probed
                   if not (clone / p).is_file() or (clone / p).read_bytes() != kept[p])
    missing = sorted(set(missing) | set(owned))
    written = sorted(set(written) | set(owned))
    failures += more + [f"probe: {f}" for f in probe_failures]
    return Comparison(written, emitted, identical, different, missing,
                      planted, restored, caught, clean, failures, len(probed))


def codegen_drift(export: pathlib.Path, binary: str, log) -> list[str]:
    sets = compass_args(("codegen",), export / ".compass.org")
    return drift_paths(compass(binary, export, ["check"] + sets, log), ("codegen",))


def describe(c: Comparison) -> str:
    lines = [f"written by the generator ({len(c.written)})"]
    lines += [f"  {p}" for p in c.written]
    lines += [f"emitted by compass ({len(c.emitted)})"]
    lines += [f"  {p}" for p in c.emitted]
    lines += [f"different ({len(c.different)})"]
    lines += [f"  {p}" for p in c.different]
    lines += [f"missing from compass ({len(c.missing)})"]
    lines += [f"  {p}" for p in c.missing]
    lines += [f"drift on the clean tree ({len(c.clean_drift)})"]
    lines += [f"  {p}" for p in c.clean_drift]
    lines += [f"planted ({len(c.planted)})"]
    lines += [f"  {p}" for p in c.planted]
    lines += [f"plants the generator rewrote that compass missed"]
    lines += [f"  {p}" for p in sorted(set(c.restored) - set(c.caught))]
    lines += [f"plants compass reported that the generator kept"]
    lines += [f"  {p}" for p in sorted(set(c.caught) - set(c.restored))]
    lines += [f"generator failures ({len(c.failures)})"]
    lines += [f"  {f}" for f in c.failures]
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--compass", required=True)
    ap.add_argument("--python", required=True)
    ap.add_argument("--work", required=True, type=pathlib.Path)
    ap.add_argument("export", type=pathlib.Path)
    ap.add_argument("names", nargs="*")
    args = ap.parse_args()
    export = args.export.resolve()
    work = args.work.resolve()
    known = {g.name for g in GENERATORS} | {"codegen"}
    unknown = sorted(set(args.names) - known)
    if unknown:
        ap.error(f"unknown generator: {', '.join(unknown)}")
    if git(export, "status", "--porcelain", "--untracked-files=no"):
        ap.error(f"{export} has uncommitted changes")
    work.mkdir(parents=True, exist_ok=True)
    clone = work / "clone"
    if not (clone / ".git").is_dir():
        subprocess.run(("git", "clone", "-q", "--local", str(export), str(clone)), check=True)
    git(clone, "fetch", "-q", str(export), "HEAD")
    git(clone, "checkout", "-q", "--detach", "FETCH_HEAD")
    ok = True
    for g in GENERATORS:
        if args.names and g.name not in args.names:
            continue
        with open(work / f"{g.name}.log", "w") as log:
            c = compare(g, export, work, clone, args.compass, args.python, log)
        (work / f"{g.name}.txt").write_text(describe(c))
        good = (not c.different and not c.missing and not c.failures
                and c.caught == c.restored)
        ok = ok and good
        print(f"{g.name}: generator wrote {len(c.written)}, compass emitted "
              f"{len(c.emitted)}, identical {len(c.identical)}, different "
              f"{len(c.different)}, missing {len(c.missing)}; plants "
              f"{len(c.planted)}, rewritten by the generator {len(c.restored)}, "
              f"drift {len(c.caught)}, agreeing "
              f"{len(c.planted) - len(set(c.caught) ^ set(c.restored))}; probed "
              f"{c.probed}; clean drift "
              f"{len(c.clean_drift)}"
              + (f"; generator failed: {'; '.join(c.failures)}" if c.failures else ""),
              flush=True)
    if not args.names or "codegen" in args.names:
        with open(work / "codegen.log", "w") as log:
            drift = codegen_drift(export, args.compass, log)
        (work / "codegen.txt").write_text(
            f"drift ({len(drift)})\n" + "".join(f"  {p}\n" for p in drift))
        print(f"codegen: drift {len(drift)}", flush=True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
