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
usage: json_codec_ore_diff.sh <old compass> <new compass> <ORE Studio git dir> <scratch dir>

Runs compass diff over a scratch copy of ORE Studio's main and compares the
new binary's report with the old one's:

  old      the old binary, as ORE Studio configures it;
  new      the new binary, as ORE Studio configures it;
  layered  the new binary, its library @compass,projects/ores.codegen/library,
           so the MASD library with the masd.cpp.codec and masd.cpp.json
           families is read beside ORE Studio's.

The new binary must sit in a bin directory with share/compass/masd beside
it, as a build or the package lays it out.
EOF
}

[ $# -eq 4 ] || { usage >&2; exit 2; }
old="$1"; new="$2"; git_dir="$3"; scratch="$4"

ore="$scratch/ore"
rm -rf "${ore:?}"
mkdir -p "$ore"
git --git-dir="$git_dir" archive main | tar -x -C "$ore"
git -C "$ore" init -q
git -C "$ore" add -A
git -C "$ore" -c user.name=scratch -c user.email=scratch@example.invalid \
    commit -qm "ORE Studio main"
echo "ORE Studio main: $(git --git-dir="$git_dir" rev-parse main)"

product=(--set codegen.product=ores
    --set component_files.library=projects/ores.codegen/library
    --set physical_inventory.directory=projects/ores.codegen/library/templates
    --set physical_inventory.product=ores
    --set component_docs.product=ores
    --set component_docs.exceptions=projects/ores.codegen/docs_exceptions.txt
    --set codegen.library=@compass,projects/ores.codegen/library)

run() {
    local name="$1"; shift
    "$@" -C "$ore" diff --files >"$scratch/$name.diff" 2>"$scratch/$name.err" ||
        true
}

same_as_old() {
    if cmp -s "$scratch/old.diff" "$scratch/$1.diff" &&
        cmp -s <(tail -n +2 "$scratch/old.err") <(tail -n +2 "$scratch/$1.err"); then
        echo "$1: same as old, $(grep -c '^+++ ' "$scratch/$1.diff") files"
    else
        echo "$1: different; see $scratch/$1.diff and $scratch/$1.err"
        return 1
    fi
}

run old "$old"
head -n 1 "$scratch/old.err"
run new "$new"
same_as_old new
run layered "$new" "${product[@]}"
same_as_old layered
