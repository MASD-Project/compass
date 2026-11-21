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
set(name "compass.cli")
set(lib_target_name ${name}.lib)
set(exe_target_name ${name}.exe)

add_library(${lib_target_name}
    app/application.cpp
    app/check_inventory.cpp
    app/codecs.cpp
    app/env.cpp
    app/pipeline.cpp
    app/steps.cpp
    config/arguments.cpp
    config/check_documents.cpp
    config/org_reader.cpp
    config/precedence.cpp
    config/repository_file.cpp
    io/hook_payload.cpp
    io/listing.cpp
    io/report.cpp
    io/version.cpp
    ops/ratchet.cpp)

set_target_properties(${lib_target_name} PROPERTIES OUTPUT_NAME ${name})

target_include_directories(${lib_target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/projects/compass.cli/include)

target_compile_definitions(${lib_target_name} PRIVATE
    COMPASS_VERSION="${PROJECT_VERSION}")

target_link_libraries(${lib_target_name}
    PUBLIC
        compass.codec.lib
        compass.codegen.lib
        compass.cpp.lib
        compass.sql.lib
        compass.crosscheck.lib
        compass.docs.lib
        compass.env.lib
        compass.model.lib
        compass.org.lib
        compass.platform.lib
        compass.store.lib
    PRIVATE
        compass.ingest.lib
        Threads::Threads)

compass_code_hash(${lib_target_name} app/code_hash.cpp
    compass::cli::app compass.cli/app/codecs.hpp)

add_executable(${exe_target_name}
    main.cpp)

set_target_properties(${exe_target_name} PROPERTIES OUTPUT_NAME compass)

target_link_libraries(${exe_target_name}
    PRIVATE
        ${lib_target_name})

# The manifest sits beside the compass binary, which finds it through its own
# resolved path. It is deliberately not a BYPRODUCT and nothing depends on it:
# a build input that changes with every commit relinks the tree. The executable
# depends on the target for ordering only, so building compass alone still
# refreshes the manifest.
add_custom_target(${name}.version ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
        "-DVERSION=${PROJECT_VERSION}"
        "-DMANIFEST_FILE=${COMPASS_PUBLISH_DIRECTORY}/bin/compass.manifest"
        -P "${CMAKE_SOURCE_DIR}/cmake/generate_version.cmake"
    COMMENT "Writing the compass build manifest"
    VERBATIM)

add_dependencies(${exe_target_name} ${name}.version)

# The release binary must run on any supported Linux, whatever libstdc++ the
# build image has, so the C++ runtime is linked in and only glibc is shared.
if(UNIX AND NOT APPLE)
    target_link_options(${exe_target_name} PRIVATE
        -static-libstdc++ -static-libgcc)
endif()

install(TARGETS ${exe_target_name} RUNTIME DESTINATION bin)
install(FILES ${COMPASS_PUBLISH_DIRECTORY}/bin/compass.manifest DESTINATION bin)

# Windows looks for a program's DLLs beside it, so the package carries the
# DLLs of the libraries compass links.
if(WIN32)
    install(FILES $<TARGET_RUNTIME_DLLS:${exe_target_name}> DESTINATION bin)
endif()
