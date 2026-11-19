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
    cat <<'USAGE'
usage: run_on_oldest_distribution.sh <archive> [<image>]

Proves a Linux release archive runs on the oldest distribution compass
supports: unpacks it in a clean container of <image> (default:
ubuntu:24.04), prints compass --version, and runs compass check over a new
repository. Needs podman or docker. Fails when either command fails.
USAGE
}

[ $# -ge 1 ] && [ "$1" != "-h" ] && [ "$1" != "--help" ] || { usage; exit 2; }
archive="$(realpath "$1")"
image="${2:-docker.io/library/ubuntu:24.04}"
engine="$(command -v podman || command -v docker)" ||
    { echo "run_on_oldest_distribution: no podman or docker" >&2; exit 2; }

"$engine" run --rm -v "$(dirname "$archive"):/archive:ro,Z" "$image" bash -euc "
    grep PRETTY_NAME /etc/os-release
    apt-get update -qq >/dev/null
    apt-get install -y -qq git >/dev/null 2>&1
    mkdir /unpacked
    tar -xzf '/archive/$(basename "$archive")' -C /unpacked
    compass=\$(find /unpacked -path '*/bin/compass' -type f)
    \"\$compass\" --version
    mkdir -p /repo/doc/meta
    cd /repo
    git init -q
    printf '#+title: A document\n' >a.org
    \"\$compass\" check -j 2 --set document_index.enabled=false \
        --set required_sections.enabled=false >/dev/null
    echo 'compass check ran on this distribution'
"
