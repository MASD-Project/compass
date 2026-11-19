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
# Run by the compass.cli.version target on every build. Writes the manifest:
# the facts that change with every commit (the commit, its time, whether the
# tree was dirty, the branch, the release tag on it) as key=value lines beside
# the compass binary,
# which reads them at runtime. The build time is not here: the binary reads it
# from its own mtime, so the manifest changes only when the commit or the
# dirty flag does.
#
# No compiled file holds these facts and nothing lists the manifest as a build
# input, so a new commit recompiles and relinks nothing. Do not make the
# manifest a BYPRODUCT or a dependency of any target, and do not substitute
# these facts into a source file: either brings back the rebuild cascade this
# script exists to prevent.
#
# Input variables (passed with -D):
#   SOURCE_DIR      - the checkout, for git
#   VERSION         - the release, PROJECT_VERSION
#   MANIFEST_FILE   - the manifest to write

cmake_minimum_required(VERSION 3.28)

# CI builds set all four; take them verbatim so the record names the CI job.
if(DEFINED ENV{COMPASS_BUILD_PROVIDER} AND
   DEFINED ENV{COMPASS_BUILD_COMMIT}   AND
   DEFINED ENV{COMPASS_BUILD_NUMBER}   AND
   DEFINED ENV{COMPASS_BUILD_TIMESTAMP})
    set(build_info "Build: Provider = '$ENV{COMPASS_BUILD_PROVIDER}'")
    string(APPEND build_info " Number = '$ENV{COMPASS_BUILD_NUMBER}'")
    string(APPEND build_info " Commit = '$ENV{COMPASS_BUILD_COMMIT}'")
    string(APPEND build_info " Timestamp = '$ENV{COMPASS_BUILD_TIMESTAMP}'")
    set(commit "$ENV{COMPASS_BUILD_COMMIT}")
    set(commit_full "$ENV{COMPASS_BUILD_COMMIT}")
    set(commit_time "")
    set(dirty "false")
    set(branch "")
    find_program(GIT_EXECUTABLE git)
    if(GIT_EXECUTABLE)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" log -1 --format=%cI
                    "$ENV{COMPASS_BUILD_COMMIT}"
            WORKING_DIRECTORY "${SOURCE_DIR}"
            OUTPUT_VARIABLE commit_time
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
    endif()
else()
    set(build_info "local unknown")
    set(commit "unknown")
    set(commit_full "unknown")
    set(commit_time "")
    set(dirty "false")
    set(branch "")
    find_program(GIT_EXECUTABLE git)
    if(GIT_EXECUTABLE)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
            WORKING_DIRECTORY "${SOURCE_DIR}"
            OUTPUT_VARIABLE git_commit
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE git_result)
        if(git_result EQUAL 0 AND NOT git_commit STREQUAL "")
            set(commit "${git_commit}")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
                WORKING_DIRECTORY "${SOURCE_DIR}"
                OUTPUT_VARIABLE commit_full
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" log -1 --format=%cI
                WORKING_DIRECTORY "${SOURCE_DIR}"
                OUTPUT_VARIABLE commit_time
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
                WORKING_DIRECTORY "${SOURCE_DIR}"
                OUTPUT_VARIABLE branch
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" status --porcelain
                WORKING_DIRECTORY "${SOURCE_DIR}"
                OUTPUT_VARIABLE git_status
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
            if(git_status STREQUAL "")
                set(build_info "local ${commit}")
            else()
                set(dirty "true")
                set(build_info "local ${commit}-dirty")
            endif()
        endif()
    endif()
endif()

# The release tag on the commit: COMPASS_BUILD_TAG when CI names one, else the
# tag git finds exactly on the commit, else none.
set(tag "")
if(DEFINED ENV{COMPASS_BUILD_TAG})
    set(tag "$ENV{COMPASS_BUILD_TAG}")
elseif(GIT_EXECUTABLE AND NOT commit_full STREQUAL "unknown")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --exact-match --tags
                "${commit_full}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()

# Written through a temporary and copied only on change, so an unchanged
# manifest keeps its mtime.
get_filename_component(manifest_dir "${MANIFEST_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${manifest_dir}")
file(WRITE "${MANIFEST_FILE}.tmp"
"format=1
version=${VERSION}
build_info=${build_info}
commit=${commit}
commit_full=${commit_full}
commit_time=${commit_time}
dirty=${dirty}
branch=${branch}
tag=${tag}
")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${MANIFEST_FILE}.tmp" "${MANIFEST_FILE}")
file(REMOVE "${MANIFEST_FILE}.tmp")
