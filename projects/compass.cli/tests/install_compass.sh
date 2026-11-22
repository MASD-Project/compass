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

# Builds of main served by a gh that answers from $root/gh: runs.txt lists
# the green Continuous Linux runs as "<id> <commit>", artifacts/<id> holds
# the package of a run that has one, tags.txt lists "<tag> <commit>" and
# assets/<tag> the asset names of a release.
if [ "$(uname -s)-$(uname -m)" = Linux-x86_64 ]; then
    gh_dir="$root/gh"
    mkdir -p "$gh_dir/bin" "$gh_dir/artifacts/101" "$gh_dir/artifacts/102" \
        "$gh_dir/assets"
    cat >"$gh_dir/bin/gh" <<EOF
#!/bin/bash
dir="$gh_dir"
EOF
    cat >>"$gh_dir/bin/gh" <<'EOF'
case "$1 $2" in
    "api repos/test/compass/commits/"*)
        grep -m1 " ${2##*/}" "$dir/runs.txt" | cut -d' ' -f2 | grep . ;;
    "api repos/test/compass/actions/workflows/"*)
        if [[ "$2" == *head_sha=* ]]; then
            grep " ${2##*head_sha=}\$" "$dir/runs.txt"
        else
            cat "$dir/runs.txt"
        fi ;;
    "api repos/test/compass/actions/runs/"*)
        id="${2#repos/test/compass/actions/runs/}"
        ls "$dir/artifacts/${id%%/*}" 2>/dev/null | grep -q . && echo 7 ;;
    "api repos/test/compass/tags"*)
        sha="$(sed -n 's/.*commit.sha == "\([0-9a-f]*\)".*/\1/p' <<<"$4")"
        grep " $sha\$" "$dir/tags.txt" | cut -d' ' -f1 ;;
    "release view") cat "$dir/assets/$3" ;;
    "run download") mkdir -p "${@: -1}" && cp "$dir/artifacts/$3"/* "${@: -1}" ;;
    *) echo "unexpected gh $*" >&2; exit 1 ;;
esac
EOF
    chmod +x "$gh_dir/bin/gh"
    export COMPASS_REPOSITORY=test/compass
    main_archive="compass-0.0.2-Linux-x86_64.tar.gz"
    pack() {
        local tree="$root/pack/$1"
        mkdir -p "$tree/compass-0.0.2/opt/compass"
        cp -rp "$(build "$1" false)/.." "$tree/compass-0.0.2/opt/compass/0.0.2"
        tar -czf "$2/$main_archive" -C "$tree" compass-0.0.2
        echo "deb" >"$2/compass_0.0.2_amd64.deb"
    }
    built=eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee
    docs=ffffffffffffffffffffffffffffffffffffffff
    pack "$built" "$gh_dir/artifacts/101"
    printf '100 %s\n101 %s\n' "$docs" "$built" >"$gh_dir/runs.txt"
    : >"$gh_dir/tags.txt"

    PATH="$gh_dir/bin:$PATH" bash "$script" --build main >"$root/out" 2>&1 ||
        fail "the newest green build of main did not install"
    expect_version "tag: none (pre-release build from https://github.com/test/compass/actions/runs/101)"
    expect_version "commit: $built (eeeeeee)"
    [ "$(cat "$masd/templates/t.mustache")" = "$built" ] ||
        fail "the MASD library of the build of main was not installed"

    bash "$script" --from "$(build $clean false)" >"$root/out" 2>&1 ||
        fail "a reinstall of the clean build did not succeed"
    expect_version "tag: none"

    PATH="$gh_dir/bin:$PATH" bash "$script" --build "${built:0:7}" \
        >"$root/out" 2>&1 || fail "the build of a commit did not install"
    expect_version "commit: $built (eeeeeee)"

    PATH="$gh_dir/bin:$PATH" bash "$script" --build "${docs:0:7}" \
        >"$root/out" 2>&1 && fail "a commit with no package installed"
    grep -q "no green Continuous Linux run of ${docs:0:7}" "$root/out" ||
        fail "a commit with no package was not named"

    pack "$clean" "$gh_dir/artifacts/102"
    echo "102 $docs" >>"$gh_dir/runs.txt"
    PATH="$gh_dir/bin:$PATH" bash "$script" --build "${docs:0:7}" \
        >"$root/out" 2>&1 && fail "a package of another commit installed"
    grep -q "built $clean, not $docs" "$root/out" ||
        fail "a package of another commit was not named"
    expect_version "commit: $built (eeeeeee)"

    echo "v0.0.1 $built" >"$gh_dir/tags.txt"
    echo "$archive" >"$gh_dir/assets/v0.0.1"
    (cd "$releases/v0.0.1" && sha256sum "$archive" >"$archive.sha256")
    PATH="$gh_dir/bin:$PATH" bash "$script" --build "${built:0:7}" \
        >"$root/out" 2>&1 || fail "the release of a built commit did not install"
    grep -qx "eeeeeee is release v0.0.1; installing the release" "$root/out" ||
        fail "the release that replaces the build was not named"
    expect_version "tag: v0.0.1"
    expect_version "commit: cccccccccccccccccccccccccccccccccccccccc (ccccccc)"
    unset COMPASS_REPOSITORY
fi
unset COMPASS_RELEASE_URL

echo "garbage" >"$opt/bin/compass.manifest"
expect_version "commit: unknown"
rm "$opt/bin/compass.manifest"
expect_version "commit: unknown"
expect_version "built: 2026-01-02T03:04:05Z"

exit 0
