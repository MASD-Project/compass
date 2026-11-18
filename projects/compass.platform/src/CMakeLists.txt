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
set(name "compass.platform")
set(lib_target_name ${name}.lib)

add_library(${lib_target_name}
    filesystem/file_lock.cpp
    filesystem/file_status.cpp
    filesystem/mapped_file.cpp
    filesystem/new_file.cpp
    process/environment.cpp
    process/executable.cpp
    process/run_program.cpp
    process/standard_streams.cpp
    text/decoding.cpp
    text/regex.cpp
    text/xml.cpp)

set_target_properties(${lib_target_name} PROPERTIES OUTPUT_NAME ${name})

target_include_directories(${lib_target_name} PUBLIC
    ${CMAKE_SOURCE_DIR}/projects/compass.platform/include)

target_link_libraries(${lib_target_name}
    PRIVATE
        PCRE2::8BIT
        expat::expat)

# The Windows implementations include windows.h, which without these defines
# pulls in most of the API and defines min and max as macros.
if(WIN32)
    target_compile_definitions(${lib_target_name} PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX)
endif()
