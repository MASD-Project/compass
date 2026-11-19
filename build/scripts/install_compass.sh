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
usage: install_compass.sh [--from <bin dir> | --release <tag> | --pin <file>]
                          [--allow-dirty]

Installs a compass build as the one compass-ng every environment shares: the
binary and its compass.manifest go into ~/.local/opt/compass/, and
~/.local/bin/compass-ng links to that binary. The link is not named compass,
because ORE Studio's Python tool answers to that name until it is retired; an
older link of ours at ~/.local/bin/compass is removed, anything else there is
left alone.

  --from <bin dir>  where the built compass and its compass.manifest are
                    (default: build/output/linux-clang-release-ninja/publish/bin)
  --release <tag>   download the archive of that release for this machine
                    from GitHub, check it against its SHA-256 checksum and
                    install it, as in --release v0.0.1
  --pin <file>      install the release a pin file names on its
                    "release: <tag>" line
  --allow-dirty     install a build of a tree with uncommitted changes

COMPASS_RELEASE_URL replaces the address releases are downloaded from,
https://github.com/MASD-Project/compass/releases/download.
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

# Downloads the archive of release $release for this machine and its
# checksum, refuses a mismatch, unpacks the archive and points from at the
# bin directory inside it.
if [ -n "$release" ]; then
    base="${COMPASS_RELEASE_URL:-https://github.com/MASD-Project/compass/releases/download}"
    archive="compass-${release#v}-$(uname -s)-$(uname -m).tar.gz"
    work="$(mktemp -d)"
    trap 'rm -rf "$work"' EXIT
    curl -fsSL -o "$work/$archive" "$base/$release/$archive" ||
        fail "cannot download $base/$release/$archive"
    curl -fsSL -o "$work/$archive.sha256" "$base/$release/$archive.sha256" ||
        fail "cannot download the checksum $base/$release/$archive.sha256"
    expected="$(cut -d' ' -f1 "$work/$archive.sha256")"
    actual="$(sha256_of "$work/$archive")"
    [ -n "$expected" ] && [ "$expected" = "$actual" ] ||
        fail "$archive does not match its checksum: expected $expected," \
             "got $actual; nothing was installed"
    mkdir "$work/unpacked"
    tar -xzf "$work/$archive" -C "$work/unpacked" ||
        fail "cannot unpack $archive"
    binary="$(find "$work/unpacked" -path '*/bin/compass' -type f | head -n 1)"
    [ -n "$binary" ] || fail "$archive holds no bin/compass"
    from="$(dirname "$binary")"
fi

manifest="$from/compass.manifest"
[ -x "$from/compass" ] || fail "no compass binary in $from; build first"
[ -f "$manifest" ] || fail "no compass.manifest in $from"
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

# Each file is written under a temporary name and renamed over the old one,
# so a running compass never sees a half-written file. cp -p keeps the
# binary's mtime, which compass --version reports as the build time.
mkdir -p "$opt" "$(dirname "$link")"
cp -p "$from/compass" "$opt/.compass.tmp.$$"
mv -f "$opt/.compass.tmp.$$" "$opt/compass"
{
    grep -v '^installed=' "$manifest"
    echo "installed=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} >"$opt/.compass.manifest.tmp.$$"
mv -f "$opt/.compass.manifest.tmp.$$" "$opt/compass.manifest"
ln -s "$opt/compass" "$link.tmp.$$"
mv -f "$link.tmp.$$" "$link"
if [ -L "$old_link" ] && [ "$(readlink "$old_link")" = "$opt/compass" ]; then
    rm -f "$old_link"
    echo "removed the old link $old_link"
fi

echo "installed $opt/compass"
echo "$link -> $opt/compass"
"$link" --version
