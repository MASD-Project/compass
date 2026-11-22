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
usage: product_root_diff.sh <old compass> <new compass> <ORE Studio git dir> <scratch dir> [<option>...]

Extracts ORE Studio's main into <scratch dir>/ore, commits it to a throwaway
repository and runs compass diff over it with the old binary as ORE Studio
configures it, then with the new binary and each <option> passed as --set.
Prints whether the two report the same files with the same bytes.
EOF
}

[ $# -ge 4 ] || { usage >&2; exit 2; }
old="$1"; new="$2"; git_dir="$3"; scratch="$4"; shift 4

ore="$scratch/ore"
rm -rf "${ore:?}"
mkdir -p "$ore"
git --git-dir="$git_dir" archive main | tar -x -C "$ore"
git -C "$ore" init -q
git -C "$ore" add -A
git -C "$ore" -c user.name=scratch -c user.email=scratch@example.invalid \
    commit -qm "ORE Studio main"

settings=()
for option in "$@"; do
    settings+=(--set "$option")
done

"$old" -C "$ore" diff --files >"$scratch/old.diff" 2>"$scratch/old.err" || true
"$new" -C "$ore" diff --files "${settings[@]}" >"$scratch/new.diff" \
    2>"$scratch/new.err" || true

head -n 1 "$scratch/old.err"
if cmp -s "$scratch/old.diff" "$scratch/new.diff" &&
    cmp -s <(tail -n +2 "$scratch/old.err") <(tail -n +2 "$scratch/new.err"); then
    echo "same: $(grep -c '^+++ ' "$scratch/old.diff") files differ from disk with either binary, byte for byte alike"
else
    echo "different: see $scratch/old.diff and $scratch/new.diff"
    exit 1
fi
