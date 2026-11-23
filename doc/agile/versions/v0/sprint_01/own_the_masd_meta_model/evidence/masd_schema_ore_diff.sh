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
usage: masd_schema_ore_diff.sh <old compass> <new compass> <ORE Studio git dir> <scratch dir>

Runs compass diff over a scratch copy of ORE Studio's main four times:

  old            the old binary, as ORE Studio configures it;
  new            the new binary, as ORE Studio configures it;
  old_layered    the old binary, its library @compass,projects/ores.codegen/library;
  new_layered    the new binary, the same, so the masd.* nodes of the MASD
                 library are in the graph beside ORE Studio's.

new must equal old, and new_layered old_layered, byte for byte. Each binary
must sit in a bin directory with share/compass/masd beside it.
EOF
}

[ $# -eq 4 ] || { usage >&2; exit 2; }
old="$1"; new="$2"; git_dir="$3"; scratch="$4"

ore="$scratch/ore"
# A copy already committed in the scratch directory is reused, so the four
# runs can be repeated without unpacking ORE Studio again.
if [ ! -d "$ore/.git" ]; then
    rm -rf "${ore:?}"
    mkdir -p "$ore"
    git --git-dir="$git_dir" archive main | tar -x -C "$ore"
    git -C "$ore" init -q
    git -C "$ore" add -A
    git -C "$ore" -c user.name=scratch -c user.email=scratch@example.invalid \
        commit -qm "ORE Studio main"
fi

layered=(--set codegen.library=@compass,projects/ores.codegen/library)

run() {
    local name="$1"; shift
    "$@" -C "$ore" diff --files >"$scratch/$name.diff" 2>"$scratch/$name.err" ||
        true
}

same() {
    if cmp -s "$scratch/$1.diff" "$scratch/$2.diff" &&
        cmp -s <(tail -n +2 "$scratch/$1.err") <(tail -n +2 "$scratch/$2.err"); then
        echo "$2: same as $1, $(grep -c '^+++ ' "$scratch/$2.diff") files"
    else
        echo "$2: different from $1; see $scratch/$2.diff and $scratch/$2.err"
        return 1
    fi
}

run old "$old"
head -n 1 "$scratch/old.err"
run new "$new"
same old new
run old_layered "$old" "${layered[@]}"
run new_layered "$new" "${layered[@]}"
same old_layered new_layered
