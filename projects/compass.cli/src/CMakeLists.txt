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
    app/codecs.cpp
    app/pipeline.cpp
    app/steps.cpp
    config/arguments.cpp
    config/precedence.cpp
    config/repository_file.cpp
    io/listing.cpp
    io/report.cpp)

set_target_properties(${lib_target_name} PROPERTIES OUTPUT_NAME ${name})

target_include_directories(${lib_target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/projects/compass.cli/include)

target_compile_definitions(${lib_target_name} PRIVATE
    COMPASS_VERSION="${PROJECT_VERSION}")

target_link_libraries(${lib_target_name}
    PUBLIC
        compass.codec.lib
        compass.codegen.lib
        compass.org.lib
        compass.platform.lib
        compass.store.lib
    PRIVATE
        compass.ingest.lib
        Threads::Threads)

add_executable(${exe_target_name}
    main.cpp)

set_target_properties(${exe_target_name} PROPERTIES OUTPUT_NAME compass)

target_link_libraries(${exe_target_name}
    PRIVATE
        ${lib_target_name})
