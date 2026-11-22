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
# hand-written manifest and a MASD library, with the install script named by
# $1, under a temporary HOME; then runs compass through the link it made.
set -u

script="$1"
compass="$2"
root="$(mktemp -d)"
trap 'rm -rf "$root"' EXIT
export HOME="$root/home"
link="$HOME/.local/bin/compass-ng"
old_link="$HOME/.local/bin/compass"
opt="$HOME/.local/opt/compass"
installed="$opt/bin/compass"
masd="$opt/share/compass/masd"

fail() {
    echo "install_compass: $*" >&2
    [ -f "$root/out" ] && cat "$root/out" >&2
    exit 1
}

# A bin directory holding a copy of the compass binary, last written at
# 2026-01-02 03:04:05 UTC, and a manifest for commit $1, dirty $2, with a
# MASD library beside it whose one template names the commit.
build() {
    local dir="$root/build/$1/bin"
    mkdir -p "$dir" "$dir/../share/compass/masd/templates"
    echo "$1" >"$dir/../share/compass/masd/templates/t.mustache"
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
mkdir -p "$HOME/.local/bin" "$opt"
echo "a plain copy" >"$link"
ln -s "$opt/compass" "$old_link"
echo "an install of the old layout" >"$opt/compass"
echo "format=1" >"$opt/compass.manifest"

no_library="$(build dddddddddddddddddddddddddddddddddddddddd false)"
rm -rf "$no_library/../share"
bash "$script" --from "$no_library" >"$root/out" 2>&1 &&
    fail "a build with no MASD library installed"
grep -q "no MASD library at" "$root/out" ||
    fail "a missing MASD library was not named"

bash "$script" --from "$(build $clean false)" >"$root/out" 2>&1 ||
    fail "a clean build did not install"
[ -L "$link" ] || fail "the plain file was not replaced by a link"
[ "$(readlink "$link")" = "$installed" ] ||
    fail "the link points at $(readlink "$link")"
[ ! -e "$old_link" ] && [ ! -L "$old_link" ] ||
    fail "the old compass link of ours was not removed"
[ ! -e "$opt/compass" ] && [ ! -e "$opt/compass.manifest" ] ||
    fail "the binary and manifest of the old layout were left"
[ "$(cat "$masd/templates/t.mustache")" = "$clean" ] ||
    fail "the MASD library was not installed beside the bin directory"
grep -qx "installed $installed" "$root/out" ||
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
[ "$(cat "$masd/templates/t.mustache")" = bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb ] ||
    fail "the MASD library of the new build did not replace the old one"
[ "$(ls -A "$opt")" = "$(printf 'bin\nshare')" ] ||
    fail "the install left more than bin and share: $(ls -A "$opt")"
[ "$(ls -A "$opt/bin")" = "$(printf 'compass\ncompass.manifest')" ] ||
    fail "bin holds more than the binary and manifest: $(ls -A "$opt/bin")"
[ "$(ls -A "$opt/share/compass")" = masd ] ||
    fail "share/compass holds more than the library: $(ls -A "$opt/share/compass")"

echo "the python tool" >"$old_link"
bash "$script" --from "$(build $clean false)" >"$root/out" 2>&1 ||
    fail "a reinstall did not succeed"
[ "$(cat "$old_link")" = "the python tool" ] ||
    fail "a compass the script does not own was touched"

# A release served from a directory: the archive of this machine, as CPack
# lays it out, and its checksum.
releases="$root/releases"
archive="compass-0.0.1-$(uname -s)-$(uname -m).tar.gz"
release_build="$(build cccccccccccccccccccccccccccccccccccccccc false)"
echo "tag=v0.0.1" >>"$release_build/compass.manifest"
mkdir -p "$root/tree/compass-0.0.1/opt/compass" "$releases/v0.0.1"
cp -rp "$release_build/.." "$root/tree/compass-0.0.1/opt/compass/0.0.1"
tar -czf "$releases/v0.0.1/$archive" -C "$root/tree" compass-0.0.1
(cd "$releases/v0.0.1" && sha256sum "$archive" >"$archive.sha256")
export COMPASS_RELEASE_URL="file://$releases"

bash "$script" --release v0.0.1 >"$root/out" 2>&1 ||
    fail "a release did not install"
expect_version "commit: cccccccccccccccccccccccccccccccccccccccc (ccccccc)"
expect_version "tag: v0.0.1"
expect_version "built: 2026-01-02T03:04:05Z"
[ "$(cat "$masd/templates/t.mustache")" = cccccccccccccccccccccccccccccccccccccccc ] ||
    fail "the MASD library of the release was not installed"

printf 'commit: x\nrelease: v0.0.1\n' >"$root/pin"
bash "$script" --from "$(build $clean false)" >"$root/out" 2>&1 ||
    fail "a reinstall of the clean build did not succeed"
bash "$script" --pin "$root/pin" >"$root/out" 2>&1 ||
    fail "the release a pin file names did not install"
expect_version "tag: v0.0.1"

bash "$script" --from "$(build $clean false)" >"$root/out" 2>&1 ||
    fail "a reinstall of the clean build did not succeed"
echo "0000000000000000000000000000000000000000000000000000000000000000  $archive" \
    >"$releases/v0.0.1/$archive.sha256"
bash "$script" --release v0.0.1 >"$root/out" 2>&1 &&
    fail "an archive that does not match its checksum installed"
grep -q "does not match its checksum" "$root/out" ||
    fail "a checksum mismatch was not named"
expect_version "commit: $clean (aaaaaaa)"
bash "$script" --release v9.9.9 >"$root/out" 2>&1 &&
    fail "a release that does not exist installed"
printf 'commit: x\n' >"$root/pin"
bash "$script" --pin "$root/pin" >"$root/out" 2>&1 &&
    fail "a pin file with no release: line installed"
unset COMPASS_RELEASE_URL

echo "garbage" >"$opt/bin/compass.manifest"
expect_version "commit: unknown"
rm "$opt/bin/compass.manifest"
expect_version "commit: unknown"
expect_version "built: 2026-01-02T03:04:05Z"

exit 0
