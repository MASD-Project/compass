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
# compass_code_hash(<target> <source> <namespace> <header>)
#
# Compiles into <target> the definition of <namespace>::code_hash(), which
# <header> declares: the hash of the code the target's codecs are computed by,
# as generate_code_hash.cmake defines it. <source> is the generated file, below
# the target's binary directory.
#
# The components and libraries hashed are those the target links, directly or
# not, so the hash follows target_link_libraries and needs no list of its own.
# They are walked at the end of the top-level directory, once every target
# exists.
#
# The hash is computed on every build. The generated source is a byproduct, so
# Ninja checks its timestamp after the command runs and, when the content was
# unchanged and the file left alone, rebuilds nothing that depends on it.

function(compass_code_hash target source namespace header)
    set_target_properties(${target} PROPERTIES
        COMPASS_CODE_HASH_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${source}"
        COMPASS_CODE_HASH_NAMESPACE "${namespace}"
        COMPASS_CODE_HASH_HEADER "${header}")
    cmake_language(EVAL CODE "
        cmake_language(DEFER DIRECTORY [[${CMAKE_SOURCE_DIR}]]
            CALL _compass_code_hash_target [[${target}]])")
endfunction()

# Appends to dirs and libraries in the caller's scope what target links:
# the root of each component of this build, and the file of each imported
# library. seen holds the targets already walked.
function(_compass_code_hash_walk target)
    get_target_property(aliased ${target} ALIASED_TARGET)
    if(aliased)
        set(target ${aliased})
    endif()
    if(target IN_LIST seen)
        return()
    endif()
    list(APPEND seen ${target})

    get_target_property(imported ${target} IMPORTED)
    if(imported)
        get_target_property(type ${target} TYPE)
        if(NOT type STREQUAL "INTERFACE_LIBRARY")
            list(APPEND libraries "$<TARGET_FILE:${target}>")
        endif()
        get_target_property(links ${target} INTERFACE_LINK_LIBRARIES)
    else()
        get_target_property(source_dir ${target} SOURCE_DIR)
        get_filename_component(component_dir "${source_dir}" DIRECTORY)
        list(APPEND dirs "${component_dir}")
        get_target_property(links ${target} LINK_LIBRARIES)
    endif()

    if(links)
        foreach(link IN LISTS links)
            string(REGEX REPLACE "^\\$<LINK_ONLY:(.*)>$" "\\1" link "${link}")
            if(TARGET ${link})
                _compass_code_hash_walk(${link})
            endif()
        endforeach()
    endif()

    set(seen ${seen} PARENT_SCOPE)
    set(dirs ${dirs} PARENT_SCOPE)
    set(libraries ${libraries} PARENT_SCOPE)
endfunction()

function(_compass_code_hash_target target)
    get_target_property(source ${target} COMPASS_CODE_HASH_SOURCE)
    get_target_property(namespace ${target} COMPASS_CODE_HASH_NAMESPACE)
    get_target_property(header ${target} COMPASS_CODE_HASH_HEADER)

    set(seen "")
    set(dirs "")
    set(libraries "")
    _compass_code_hash_walk(${target})
    list(REMOVE_DUPLICATES dirs)
    list(REMOVE_DUPLICATES libraries)
    string(JOIN "|" dirs ${dirs})
    string(JOIN "|" libraries ${libraries})

    add_custom_target(${target}.code_hash
        COMMAND "${CMAKE_COMMAND}"
            "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
            "-DCOMPONENT_DIRS=${dirs}"
            "-DLIBRARY_FILES=${libraries}"
            "-DTOOLCHAIN=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
            "-DNAMESPACE=${namespace}"
            "-DHEADER=${header}"
            "-DOUTPUT_FILE=${source}"
            -P "${CMAKE_SOURCE_DIR}/cmake/generate_code_hash.cmake"
        BYPRODUCTS "${source}"
        COMMENT "Hashing the code of ${target}"
        VERBATIM)

    target_sources(${target} PRIVATE "${source}")
    add_dependencies(${target} ${target}.code_hash)
endfunction()
