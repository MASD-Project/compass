#!/bin/sh
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
# compiler_cache_wrapper.sh — compiler launcher wrapper for sccache/ccache.
#
# Wired in unconditionally via CMakePresets.json (so it wins the race against
# vcpkg's own compiler-detection bootstrap, which otherwise invokes a bare
# `sccache`/`ccache` before this project's CMakeLists.txt gets a chance to
# guard on find_program()). Because it's unconditional, this script itself
# must gate activation explicitly — it must NOT activate a compiler cache
# just because one happens to be in PATH for unrelated reasons on the
# machine. Activation requires an explicit opt-in:
#
#   - CI: always sccache (CI sets SCCACHE_DIR directly).
#   - Local dev: only if COMPASS_COMPILER_CACHE=sccache or =ccache is set in
#     .env at the root of the checkout.
#   - Anything else (no .env, or .env without COMPASS_COMPILER_CACHE): fall
#     through to the real compiler untouched.
#
# For sccache: sets SCCACHE_BASEDIR to the project root before invoking it,
# so that absolute paths in compiler invocations are normalised to relative
# paths before the cache key is computed. This makes all git worktrees of the
# same commit share a single sccache entry per translation unit, eliminating
# redundant full rebuilds across environments. SCCACHE_DIR/SCCACHE_CACHE_SIZE
# are not set here — sccache picks them up from its own global config
# (~/.config/sccache/config).
#
# For ccache: cache location and tuning (direct_mode, hash_dir, hard_link,
# max_size) come from ccache's own global config (~/.config/ccache/ccache.conf).
# CCACHE_BASEDIR is set here for the same cross-worktree sharing reason as
# SCCACHE_BASEDIR above; hash_dir is disabled in ccache.conf so that base_dir
# normalisation actually takes effect (hash_dir would otherwise bake each
# worktree's absolute build directory into every object's hash).
#
# Used as CMAKE_C_COMPILER_LAUNCHER / CMAKE_CXX_COMPILER_LAUNCHER in
# CMakeLists.txt.  The project root is resolved at runtime from this script's
# own location (build/scripts/ is two levels below the root), so the path is
# always correct regardless of which worktree or machine runs the build.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

if [ -n "${CI:-}" ]; then
    TOOL="sccache"
elif [ -f "${PROJECT_ROOT}/.env" ]; then
    TOOL="$(sed -n 's/^COMPASS_COMPILER_CACHE=//p' "${PROJECT_ROOT}/.env" | tail -n1)"
fi

if [ -z "${TOOL:-}" ] || ! command -v "${TOOL}" >/dev/null 2>&1; then
    exec "$@"
fi

# A precompiled header records the absolute paths of the headers it read, and
# base_dir does not rewrite them. A cached one from another worktree points at
# that worktree's headers, and the compiler refuses it once one of them changes
# there. CI has one checkout per job, so its cached headers stay valid.
if [ -z "${CI:-}" ]; then
    prev=""
    for arg in "$@"; do
        case "${prev}:${arg}" in
            *:-emit-pch | -o:*.pch | -o:*.gch | *:-o*.pch | *:-o*.gch) exec "$@" ;;
        esac
        prev="${arg}"
    done
fi

if [ "${TOOL}" = "sccache" ]; then
    export SCCACHE_BASEDIR="${SCCACHE_BASEDIR:-${PROJECT_ROOT}}"
elif [ "${TOOL}" = "ccache" ]; then
    export CCACHE_BASEDIR="${CCACHE_BASEDIR:-${PROJECT_ROOT}}"
fi

exec "${TOOL}" "$@"
