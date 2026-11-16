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
# Runs the compass executable named by $1 over a fresh git repository:
# generate, diff, check, then check again after fixing the one finding.
set -u

compass="$1"
root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT
export HOME="$root/home" XDG_CONFIG_HOME="$root/home/.config"
export GIT_CONFIG_NOSYSTEM=1

fail() {
    echo "end_to_end: $*" >&2
    exit 1
}

expect() {
    local want="$1" name="$2"
    shift 2
    "$@" >"$root/out" 2>"$root/err"
    local got=$?
    [ "$got" -eq "$want" ] || {
        cat "$root/out" "$root/err" >&2
        fail "$name exited $got, expected $want"
    }
}

repo="$root/repo"
mkdir -p "$repo/doc/meta" "$HOME"
cat >"$repo/doc/meta/document_type_task.org" <<'EOF'
#+title: Document type: task
#+type: meta

* Contract

- Required sections ::
  - =* Goal= — one paragraph.
  - =* Result= — filled at the end.
EOF
printf '#+title: Task: a\n#+type: task\n* Goal\n' >"$repo/doc/task_a.org"
git -C "$repo" init -q || fail "git init failed"
git -C "$repo" add -A || fail "git add failed"

cd "$repo" || fail "no repository"

expect 0 "generate" "$compass" generate
grep -q '^1 written, ' "$root/out" || fail "generate wrote no file"
[ -f build/output/compass/doc/document_index.tsv ] ||
    fail "the index is not under the default output root"

expect 0 "diff" "$compass" diff
[ -s "$root/out" ] && fail "diff printed a diff after generate"

expect 1 "check" "$compass" check
grep -q '^doc/task_a.org:2:9: required_sections/missing_section: ' \
    "$root/out" || fail "check did not report the missing section"

printf '#+title: Task: a\n#+type: task\n* Goal\n* Result\n' \
    >"$repo/doc/task_a.org"
expect 0 "check after the fix" "$compass" check

expect 2 "an unknown command" "$compass" write
exit 0
