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
"""Triage MSVC's C1202 failures from the failed-job logs of CI runs.

For each C1202 a log holds, it reports the job, the object ninja failed on
(and whether it is a unity unit), the header and line the error is in, the
project source the instantiation context starts from, the deepest project
source it passes, the ORE Studio types the context names, and how many
context notes MSVC printed. Fetch the logs first, one gzipped file per run:

    gh run view <run> -R OreStudio/OreStudio --log-failed | gzip \
        > <dir>/<run>.txt.gz

    python3 build/scripts/triage_c1202.py <dir> > triage.tsv
"""

from __future__ import annotations

import gzip
import pathlib
import re
import sys

ANSI = re.compile(r"\^\[\[[0-9;]*m|\x1b\[[0-9;]*m")
STAMP = re.compile(r"^\S+\t[^\t]*\t\d{4}-\d\d-\d\dT[\d:.]+Z ?")
FAILED = re.compile(r"^FAILED: (?:\[code=\d+\] )?(\S+)")
ERROR = re.compile(r"(?:##\[error\])?([^\s]+)\((\d+)\): fatal error C1202")
NOTE = re.compile(r"^([A-Za-z]:\\[^(]+)\((\d+)\): note: (.*)")
PROJECT = re.compile(r"\\projects\\(.+)$")
ORE_TYPE = re.compile(r"ores::[a-z_]+::[a-z_:]*[a-z_]+")
COLUMNS = ("run", "job", "object", "unity", "error_at", "starts_in",
           "deepest_project", "notes", "types")


def project_path(path: str) -> str | None:
    m = PROJECT.search(path.replace("/", "\\"))
    return m.group(1).replace("\\", "/") if m else None


def triage(run: str, lines: list[str]):
    obj = ""
    for i, line in enumerate(lines):
        f = FAILED.match(line)
        if f:
            obj = f.group(1)
        e = ERROR.search(line)
        if not e:
            continue
        header = e.group(1).replace("\\", "/")
        header = header[header.find("include/") + 8:] \
            if "include/" in header else header
        notes, sources, types = 0, [], []
        for follow in lines[i + 1:]:
            n = NOTE.match(follow)
            if n:
                notes += 1
                p = project_path(n.group(1))
                if p:
                    sources.append(f"{p}:{n.group(2)}")
            elif not follow.startswith((" ", "\t")) and follow.strip():
                break
            for t in ORE_TYPE.findall(follow):
                if t not in types:
                    types.append(t)
        yield {
            "run": run,
            "job": "",
            "object": obj,
            "unity": "yes" if "unity_" in obj.lower() else "no",
            "error_at": f"{header}:{e.group(2)}",
            "starts_in": sources[0] if sources else "",
            "deepest_project": sources[-1] if sources else "",
            "notes": str(notes),
            "types": " ".join(types[:8]),
        }


def main() -> int:
    print("\t".join(COLUMNS))
    for path in sorted(pathlib.Path(sys.argv[1]).glob("*.txt.gz")):
        run = path.name.split(".")[0]
        text = gzip.decompress(path.read_bytes()).decode(errors="replace")
        jobs = {}
        for raw in text.splitlines():
            job = raw.split("\t", 1)[0]
            jobs.setdefault(job, []).append(
                STAMP.sub("", ANSI.sub("", raw)))
        for job, lines in jobs.items():
            for row in triage(run, lines):
                row["job"] = job
                print("\t".join(row[c] for c in COLUMNS))
    return 0


if __name__ == "__main__":
    sys.exit(main())
