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
usage: install_compass.sh [--from <bin dir>] [--allow-dirty]

Installs a compass build as the one compass every environment shares: the
binary and its compass.manifest go into ~/.local/opt/compass/, and
~/.local/bin/compass links to that binary.

  --from <bin dir>  where the built compass and its compass.manifest are
                    (default: build/output/linux-clang-release/publish/bin)
  --allow-dirty     install a build of a tree with uncommitted changes
EOF
}

fail() {
    echo "install_compass: $*" >&2
    exit 1
}

repository="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
from="$repository/build/output/linux-clang-release/publish/bin"
opt="$HOME/.local/opt/compass"
link="$HOME/.local/bin/compass"
allow_dirty=false

while [ $# -gt 0 ]; do
    case "$1" in
        --from) [ $# -ge 2 ] || fail "--from needs a directory"
                from="$2"; shift ;;
        --allow-dirty) allow_dirty=true ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
    shift
done

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
mv -T "$link.tmp.$$" "$link"

echo "installed $opt/compass"
echo "$link -> $opt/compass"
"$link" --version
