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
# Installs copies of the compass executable named by $2, each beside a
# hand-written manifest, with the install script named by $1, under a
# temporary HOME; then runs compass through the link it made.
set -u

script="$1"
compass="$2"
root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT
export HOME="$root/home"
link="$HOME/.local/bin/compass"
opt="$HOME/.local/opt/compass"

fail() {
    echo "install_compass: $*" >&2
    [ -f "$root/out" ] && cat "$root/out" >&2
    exit 1
}

# A bin directory holding a copy of the compass binary, last written at
# 2026-01-02 03:04:05 UTC, and a manifest for commit $1, dirty $2.
build() {
    local dir="$root/build/$1"
    mkdir -p "$dir"
    cp "$compass" "$dir/compass"
    touch -d 2026-01-02T03:04:05Z "$dir/compass"
    cat >"$dir/compass.manifest" <<EOF
format=1
version=0.0.1
build_info=local ${1:0:7}
commit=${1:0:7}
commit_full=$1
commit_time=2026-01-01T10:00:00+00:00
dirty=$2
branch=main
EOF
    echo "$dir"
}

expect_version() {
    "$link" --version >"$root/out" || fail "compass --version failed"
    grep -qx "$1" "$root/out" || fail "--version lacks '$1'"
}

clean=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
mkdir -p "$HOME/.local/bin"
echo "a plain copy" >"$link"

bash "$script" --from "$(build $clean false)" >"$root/out" 2>&1 ||
    fail "a clean build did not install"
[ -L "$link" ] || fail "the plain file was not replaced by a link"
[ "$(readlink "$link")" = "$opt/compass" ] ||
    fail "the link points at $(readlink "$link")"
grep -qx "installed $opt/compass" "$root/out" ||
    fail "the installed path was not printed"
grep -qx "commit: $clean (aaaaaaa)" "$root/out" ||
    fail "the --version output was not printed"
expect_version "commit: $clean (aaaaaaa)"
expect_version "dirty: no"
expect_version "committed: 2026-01-01T10:00:00+00:00"
expect_version "built: 2026-01-02T03:04:05Z"
grep -q '^installed: 20[0-9-]*T[0-9:]*Z$' "$root/out" ||
    fail "--version does not name the install time"

dirty="$(build bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb true)"
bash "$script" --from "$dirty" >"$root/out" 2>&1 &&
    fail "a dirty build installed without --allow-dirty"
expect_version "commit: $clean (aaaaaaa)"
bash "$script" --from "$dirty" --allow-dirty >"$root/out" 2>&1 ||
    fail "a dirty build did not install with --allow-dirty"
expect_version "dirty: yes"
[ "$(ls -A "$opt")" = "$(printf 'compass\ncompass.manifest')" ] ||
    fail "the install left more than the binary and manifest: $(ls -A "$opt")"

echo "garbage" >"$opt/compass.manifest"
expect_version "commit: unknown"
rm "$opt/compass.manifest"
expect_version "commit: unknown"
expect_version "built: 2026-01-02T03:04:05Z"

exit 0
