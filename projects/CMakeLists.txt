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
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(flags "")
    if(WITH_PROFILING)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            set(flags "${flags} -fprofile-abs-path")
        endif()
        set(flags "${flags} -fprofile-arcs")
        set(flags "${flags} -ftest-coverage")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flags}")
    message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
endif()

# -Werror without -Wall only escalates the handful of diagnostics on by
# default, which is how a non-exhaustive switch over an enum class
# compiled clean here for years: -Wswitch arrives with -Wall.
#
# Two of -Wextra's warnings are switched back off. Both fire on
# aggregate initialisation that leaves a trailing field to its default
# value — reply(nats_, msg, get_images_response{false, e.what()}) and
# the like — which is deliberate here and not a defect. They accounted
# for 2040 of the 2153 warnings the first full sweep found, against 113
# that named something real, so leaving them on would have buried the
# signal rather than surfaced it.
#
# What this gives up, stated plainly because it is what a future
# missing-field bug will turn on: adding a field to an aggregate no
# longer errors at the call sites that do not set it. They keep
# compiling and the new field is value-initialised. If a field added to
# a struct is silently default somewhere it should not be, this is the
# warning that would have caught it.
#
# The designated-initializer variant is Clang's alone, hence the
# separate guard.
#
# COMPASS_WARNINGS_AS_ERRORS sets -Werror and /WX. It is off until every
# platform builds clean of warnings, so a warning reaches CDash as a warning
# and the tests still run.
add_compile_options(
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wall>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wextra>
    # MSVC's /W4 warns when a declaration hides a local, a parameter, a
    # member or a global (C4456 to C4459); without -Wshadow the Linux build
    # passed what only the Windows nightly then reported.
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wshadow>
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wno-missing-field-initializers>
    $<$<CXX_COMPILER_ID:Clang>:-Wno-missing-designated-field-initializers>
    # gcc 16 reports -Warray-bounds for code this tree does not own: the control
    # block libstdc++'s make_shared builds, and boost 1.92's exception and beast
    # internals (make_shared_object.hpp, shared_count.hpp). Every array-bounds
    # diagnostic the gcc 16 build reports is one of those, so the warning stays
    # off until the compiler or boost stops reporting it. The cost, stated for
    # the day this changes: a genuine out-of-bounds read in our own code no
    # longer reaches the build log.
    $<$<CXX_COMPILER_ID:GNU>:-Wno-array-bounds>
    $<$<AND:$<BOOL:${COMPASS_WARNINGS_AS_ERRORS}>,$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:-Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<AND:$<BOOL:${COMPASS_WARNINGS_AS_ERRORS}>,$<CXX_COMPILER_ID:MSVC>>:/WX>
    # C4251 fires on every exported class that carries a std::string or a
    # container, which is most of them. Every consumer is an in-tree target
    # built by this same toolchain against these same headers, so the ABI
    # mismatch the warning exists to catch cannot arise here.
    $<$<CXX_COMPILER_ID:MSVC>:/wd4251>
    # C4275 is C4251's counterpart for base classes. The exception types
    # exported by ores.platform derive from std::exception and
    # boost::exception, which carry no dll-interface. Dropping the bases
    # to silence it would cost every catch site its standard handling.
    $<$<CXX_COMPILER_ID:MSVC>:/wd4275>
    # /FS forces synchronous PDB writes so sccache can cache /Zi compilations.
    # Without it, parallel cl.exe invocations race on the shared .pdb file.
    $<$<CXX_COMPILER_ID:MSVC>:/FS>)

# Test cases tagged [posix] check what only a POSIX system provides, such as
# inode numbers, so the Windows runs leave them out.
if(WIN32)
    set(COMPASS_TEST_FILTER "~[posix]")
else()
    set(COMPASS_TEST_FILTER "")
endif()

# Each Catch2 test case is registered as its own CTest test, named
# <suite>.<test case>, so CDash reports every case's result and time rather than
# one entry per suite. Each is labelled with its component, and the components
# are written to cdash_subprojects.txt, which CTest.cmake submits as CDash
# subprojects, so CDash groups the cases by component.
# Discovery runs at test time, so building never runs a test binary.
include(Catch)
function(compass_add_catch2_tests suite target)
    set(spec "")
    if(COMPASS_TEST_FILTER)
        set(spec TEST_SPEC ${COMPASS_TEST_FILTER})
    endif()
    catch_discover_tests(${target}
        ${spec}
        TEST_PREFIX "${suite}."
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        DISCOVERY_MODE PRE_TEST
        PROPERTIES LABELS ${suite})
    set_property(GLOBAL APPEND PROPERTY COMPASS_TEST_COMPONENTS ${suite})
endfunction()

# Components are added by name, in dependency order. compass.legacy and
# modeling are not C++ components and are never added here.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.platform)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.testing)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.store)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.org)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.ingest)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.db)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.model)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.codegen)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.codec)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.cpp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.sql)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.env)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.crosscheck)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.docs)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.cli)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/compass.bench)

get_property(test_components GLOBAL PROPERTY COMPASS_TEST_COMPONENTS)
list(JOIN test_components "\n" test_components)
file(WRITE ${CMAKE_BINARY_DIR}/cdash_subprojects.txt "${test_components}\n")
