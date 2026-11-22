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
usage: install_compass.sh [--from <bin dir> | --release <tag> | --pin <file> |
                           --build <commit|main>] [--allow-dirty]

Installs a compass build as the one compass-ng every environment shares,
laid out as the package lays it out: the binary and its compass.manifest go
into ~/.local/opt/compass/bin/, the MASD library beside the bin directory
into ~/.local/opt/compass/share/compass/masd/, and ~/.local/bin/compass-ng
links to the binary. The link is not named compass,
because ORE Studio's Python tool answers to that name until it is retired; an
older link of ours at ~/.local/bin/compass is removed, anything else there is
left alone.

  --from <bin dir>  where the built compass and its compass.manifest are,
                    with the MASD library at ../share/compass/masd
                    (default: build/output/linux-clang-release-ninja/publish/bin)
  --release <tag>   download the archive of that release for this machine
                    from GitHub, check it against its SHA-256 checksum and
                    install it, as in --release v0.0.1
  --pin <file>      install the release a pin file names on its
                    "release: <tag>" line
  --build <commit|main>
                    install the Linux package Continuous Linux built of that
                    commit, or of the newest commit of main it built green,
                    before the release of it is out. It needs gh, logged in.
                    --version then says the build is a pre-release. When the
                    commit already has a release with its archive, that
                    release is installed instead.
  --allow-dirty     install a build of a tree with uncommitted changes

COMPASS_RELEASE_URL replaces the address releases are downloaded from,
https://github.com/MASD-Project/compass/releases/download. COMPASS_REPOSITORY
replaces the repository --build reads, MASD-Project/compass.
EOF
}

fail() {
    echo "install_compass: $*" >&2
    exit 1
}

repository="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
from="$repository/build/output/linux-clang-release-ninja/publish/bin"
opt="$HOME/.local/opt/compass"
link="$HOME/.local/bin/compass-ng"
old_link="$HOME/.local/bin/compass"
allow_dirty=false
release=""
build=""
prerelease=""

while [ $# -gt 0 ]; do
    case "$1" in
        --from) [ $# -ge 2 ] || fail "--from needs a directory"
                from="$2"; shift ;;
        --release) [ $# -ge 2 ] || fail "--release needs a tag"
                   release="$2"; shift ;;
        --pin) [ $# -ge 2 ] || fail "--pin needs a file"
               [ -f "$2" ] || fail "no pin file $2"
               release="$(sed -n 's/^release:[[:space:]]*//p' "$2" | head -n 1)"
               [ -n "$release" ] || fail "$2 has no release: line"
               shift ;;
        --build) [ $# -ge 2 ] || fail "--build needs a commit or main"
                 build="$2"; shift ;;
        --allow-dirty) allow_dirty=true ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
    shift
done

sha256_of() {
    if command -v sha256sum >/dev/null; then
        sha256sum "$1" | cut -d' ' -f1
    else
        shasum -a 256 "$1" | cut -d' ' -f1
    fi
}

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Unpacks the archive $1 and points from at the bin directory inside it.
unpack() {
    mkdir "$work/unpacked"
    tar -xzf "$1" -C "$work/unpacked" || fail "cannot unpack $(basename "$1")"
    binary="$(find "$work/unpacked" -path '*/bin/compass' -type f | head -n 1)"
    [ -n "$binary" ] || fail "$(basename "$1") holds no bin/compass"
    from="$(dirname "$binary")"
}

# Finds the Continuous Linux run that built $build green and holds its
# package, and points from at the package's bin directory. A commit whose
# release already carries its archive installs that release instead, so a
# pre-release build never stands in for a release that is out.
if [ -n "$build" ]; then
    [ "$(uname -s)-$(uname -m)" = Linux-x86_64 ] ||
        fail "--build installs the Linux x86_64 package only"
    command -v gh >/dev/null || fail "--build needs gh"
    repo="${COMPASS_REPOSITORY:-MASD-Project/compass}"
    runs="repos/$repo/actions/workflows/continuous-linux.yml/runs?status=success&per_page=50"
    if [ "$build" = main ]; then
        runs="$runs&branch=main"
    else
        sha="$(gh api "repos/$repo/commits/$build" --jq .sha)" ||
            fail "gh cannot name the commit $build of $repo"
        runs="$runs&head_sha=$sha"
    fi
    run=""
    while read -r id head; do
        [ -n "$id" ] || continue
        if [ -n "$(gh api "repos/$repo/actions/runs/$id/artifacts" --jq \
                '.artifacts[] | select(.name == "linux-clang-release-ninja" and (.expired | not)) | .id')" ]; then
            run="$id"; sha="$head"; break
        fi
    done < <(gh api "$runs" --jq '.workflow_runs[] | "\(.id) \(.head_sha)"')
    [ -n "$run" ] || fail "no green Continuous Linux run of $build holds" \
                         "its Linux package"
    for tag in $(gh api "repos/$repo/tags?per_page=100" --jq \
                     ".[] | select(.commit.sha == \"$sha\") | .name"); do
        assets="$(gh release view "$tag" --repo "$repo" --json assets \
                      --jq '.assets[].name' 2>/dev/null || true)"
        if grep -qx "compass-${tag#v}-Linux-x86_64.tar.gz" <<<"$assets"; then
            echo "${sha:0:7} is release $tag; installing the release"
            release="$tag"
            break
        fi
    done
    if [ -z "$release" ]; then
        gh run download "$run" --repo "$repo" \
            --name linux-clang-release-ninja --dir "$work/build" >/dev/null ||
            fail "cannot download the package of run $run"
        archive="$(find "$work/build" -name 'compass-*-Linux-x86_64.tar.gz' | head -n 1)"
        [ -n "$archive" ] || fail "run $run holds no Linux archive"
        unpack "$archive"
        prerelease="https://github.com/$repo/actions/runs/$run"
    fi
fi

# Downloads the archive of release $release for this machine and its
# checksum, refuses a mismatch, unpacks the archive and points from at the
# bin directory inside it.
if [ -n "$release" ]; then
    base="${COMPASS_RELEASE_URL:-https://github.com/MASD-Project/compass/releases/download}"
    archive="compass-${release#v}-$(uname -s)-$(uname -m).tar.gz"
    curl -fsSL -o "$work/$archive" "$base/$release/$archive" ||
        fail "cannot download $base/$release/$archive"
    curl -fsSL -o "$work/$archive.sha256" "$base/$release/$archive.sha256" ||
        fail "cannot download the checksum $base/$release/$archive.sha256"
    expected="$(cut -d' ' -f1 "$work/$archive.sha256")"
    actual="$(sha256_of "$work/$archive")"
    [ -n "$expected" ] && [ "$expected" = "$actual" ] ||
        fail "$archive does not match its checksum: expected $expected," \
             "got $actual; nothing was installed"
    unpack "$work/$archive"
fi

manifest="$from/compass.manifest"
masd="$from/../share/compass/masd"
[ -x "$from/compass" ] || fail "no compass binary in $from; build first"
[ -f "$manifest" ] || fail "no compass.manifest in $from"
[ -d "$masd" ] || fail "no MASD library at $masd"
value() { sed -n "s/^$1=//p" "$manifest" | head -n 1; }
[ "$(value format)" = 1 ] || fail "unknown manifest format in $manifest"
commit="$(value commit_full)"
case "$commit" in ''|unknown) fail "$manifest names no commit" ;; esac
case "$(value dirty)" in
    false) ;;
    true) "$allow_dirty" ||
              fail "the build of $commit is dirty; commit and rebuild, or" \
                   "pass --allow-dirty" ;;
    *) fail "the dirty flag in $manifest is neither true nor false" ;;
esac
[ -z "$prerelease" ] || [ "$commit" = "$sha" ] ||
    fail "run ${prerelease##*/} built $commit, not $sha; nothing was installed"

# Each file is written under a temporary name and renamed over the old one,
# so a running compass never sees a half-written file. cp -p keeps the
# binary's mtime, which compass --version reports as the build time. The
# library is copied whole beside the old one and then swapped in.
bin="$opt/bin"
share="$opt/share/compass"
mkdir -p "$bin" "$share" "$(dirname "$link")"
cp -p "$from/compass" "$bin/.compass.tmp.$$"
mv -f "$bin/.compass.tmp.$$" "$bin/compass"
{
    grep -v '^installed=\|^prerelease=' "$manifest"
    [ -z "$prerelease" ] || echo "prerelease=$prerelease"
    echo "installed=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} >"$bin/.compass.manifest.tmp.$$"
mv -f "$bin/.compass.manifest.tmp.$$" "$bin/compass.manifest"
rm -rf "${share:?}/.masd.tmp.$$"
cp -Rp "$masd" "$share/.masd.tmp.$$"
rm -rf "${share:?}/masd"
mv "$share/.masd.tmp.$$" "$share/masd"
ln -s "$bin/compass" "$link.tmp.$$"
mv -f "$link.tmp.$$" "$link"
rm -f "$opt/compass" "$opt/compass.manifest"
if [ -L "$old_link" ]; then
    case "$(readlink "$old_link")" in
        "$opt/compass"|"$bin/compass")
            rm -f "$old_link"
            echo "removed the old link $old_link" ;;
    esac
fi

echo "installed $bin/compass"
echo "$link -> $bin/compass"
"$link" --version
