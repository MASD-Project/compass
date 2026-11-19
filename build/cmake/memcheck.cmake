# -*- mode: cmake; cmake-tab-width: 4; indent-tabs-mode: nil -*-
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
#
# The memcheck command line every memcheck run uses: the nightly's
# dashboard script (CTest.cmake) and a local run (build/scripts/run_memcheck.sh)
# both include this file, so the two cannot drift apart.
#
# The end-to-end and install tests are shell scripts that run compass from the
# build tree. Memcheck follows compass into those children but not the shell,
# git and the other system programs the scripts start, whose allocations are
# not compass's.
#
set(COMPASS_MEMCHECK_OPTIONS
    --trace-children=yes
    --trace-children-skip=/usr/*,/bin/*
    --quiet
    --tool=memcheck
    --leak-check=full
    --show-reachable=yes
    --num-callers=50
    --demangle=yes
    --gen-suppressions=all)
list(JOIN COMPASS_MEMCHECK_OPTIONS " " COMPASS_MEMCHECK_OPTIONS)
set(COMPASS_MEMCHECK_SUPPRESSIONS
    "${CMAKE_CURRENT_LIST_DIR}/../valgrind/custom.supp")
cmake_path(NORMAL_PATH COMPASS_MEMCHECK_SUPPRESSIONS)

if(COMPASS_PRINT_MEMCHECK)
    message("${COMPASS_MEMCHECK_OPTIONS}")
    message("${COMPASS_MEMCHECK_SUPPRESSIONS}")
endif()
