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
usage: run_memcheck.sh <build dir> [<ctest options>...]

Runs the tests of a build under valgrind's memcheck as the nightly does, with
the options and suppressions of build/cmake/memcheck.cmake, then checks every
stack memcheck reported against the suppression file. Pass ctest options such
as -R <regex> or -j <n> to narrow or widen the run; the default is -j 2.

The logs stay in <build dir>/Testing/Temporary/MemoryChecker.*.log, and every
reported stack is gathered into <build dir>/Testing/Temporary/memcheck.log.
USAGE
}

[ $# -ge 1 ] && [ "$1" != "-h" ] && [ "$1" != "--help" ] || { usage; exit 2; }
build=$(realpath "$1")
shift
source_dir=$(realpath "$(dirname "$0")/../..")
valgrind=$(command -v valgrind) || { echo "run_memcheck: valgrind not found" >&2; exit 2; }

mapfile -t memcheck < <(cmake -DCOMPASS_PRINT_MEMCHECK=ON \
    -P "$source_dir/build/cmake/memcheck.cmake" 2>&1)

cat > "$build/DartConfiguration.tcl" <<DART
SourceDirectory: $source_dir
BuildDirectory: $build
MemoryCheckType: Valgrind
MemoryCheckCommand: $valgrind
MemoryCheckCommandOptions: ${memcheck[0]}
MemoryCheckSuppressionFile: ${memcheck[1]}
DART

args=("$@")
[ ${#args[@]} -gt 0 ] || args=(-j 2)
rm -f "$build"/Testing/Temporary/MemoryChecker.*.log
status=0
ctest --test-dir "$build" -T MemCheck "${args[@]}" || status=$?

cat "$build"/Testing/Temporary/MemoryChecker.*.log \
    > "$build/Testing/Temporary/memcheck.log" 2>/dev/null || true
python3 "$source_dir/build/scripts/check_valgrind_suppressions.py" \
    "${memcheck[1]}" "$build/Testing/Temporary/memcheck.log" || status=1
exit $status
