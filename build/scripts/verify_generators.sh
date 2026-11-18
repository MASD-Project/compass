#!/bin/bash
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
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: verify_generators.sh [--compass BIN] [--work DIR] [--keep] EXPORT [NAME...]

Runs each ORE Studio generator compass replaces on a copy of EXPORT and
compares what it writes with what compass emits on EXPORT, then plants a line
in each output and checks compass reports it as drift. Prints one line per
generator; the paths behind each count are in WORK/<name>.txt.

EXPORT   a git repository holding an ORE Studio tree, for example:
           git archive --remote=<ores.git> origin/main | tar -x -C EXPORT
           git -C EXPORT init && git -C EXPORT add -A && git -C EXPORT commit -m export
NAME     limit the run to these generators; codegen names the codegen check.

--compass BIN  the compass binary (default: this repository's release build)
--work DIR     scratch directory for the clone, the venv and the outputs
               (default: EXPORT.verify)
--keep         keep the clone and the outputs in WORK when done
EOF
}

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$here/../.." && pwd)
compass="$root/build/output/linux-clang-release-ninja/publish/bin/compass"
work=""
keep=no
while [ $# -gt 0 ]; do
    case $1 in
        --compass) compass=$2; shift 2;;
        --work) work=$2; shift 2;;
        --keep) keep=yes; shift;;
        -h|--help) usage; exit 0;;
        -*) usage >&2; exit 2;;
        *) break;;
    esac
done
[ $# -ge 1 ] || { usage >&2; exit 2; }
export_dir=$(cd "$1" && pwd)
shift
work=${work:-$export_dir.verify}
[ -x "$compass" ] || { echo "verify_generators: no compass binary at $compass" >&2; exit 2; }
mkdir -p "$work"

venv=$work/venv
if [ ! -x "$venv/bin/python" ]; then
    python3 -m venv "$venv"
    "$venv/bin/pip" install -q -r "$export_dir/projects/ores.codegen/requirements.txt"
fi

# A codec changed under an unchanged version would replay results stored by an
# older binary, so every run starts from an empty store.
rm -rf "$export_dir/.git/compass"
status=0
"$venv/bin/python" -I -B "$here/verify_generators.py" --compass "$compass" \
    --python "$venv/bin/python" --work "$work" "$export_dir" "$@" || status=$?
if [ $keep = no ]; then
    rm -rf "$work/clone" "$work/out"
fi
exit $status
