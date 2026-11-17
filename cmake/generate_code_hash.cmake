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
# Run by a component's code hash target on every build. Writes a source file
# defining code_hash(): the SHA-256 of everything the component's codecs are
# computed by. That is every file under include/ and src/ of the component and
# of each component it links, named by its path below SOURCE_DIR so every
# checkout of the same code agrees; the bytes of each third-party library it
# links, named by file name; and the compiler.
#
# The source is written through a temporary and copied only when it changes,
# so a build that changed none of those inputs recompiles and relinks nothing.
#
# Input variables (passed with -D), lists separated by '|':
#   SOURCE_DIR      - the checkout
#   COMPONENT_DIRS  - the root of each component hashed
#   LIBRARY_FILES   - each third-party library file linked
#   TOOLCHAIN       - the compiler's identity and version
#   NAMESPACE       - the namespace code_hash() is declared in
#   HEADER          - the header that declares it
#   OUTPUT_FILE     - the source to write

cmake_minimum_required(VERSION 3.28)

string(REPLACE "|" ";" component_dirs "${COMPONENT_DIRS}")
string(REPLACE "|" ";" library_files "${LIBRARY_FILES}")

set(files "")
foreach(dir IN LISTS component_dirs)
    file(GLOB_RECURSE found LIST_DIRECTORIES false
        "${dir}/include/*" "${dir}/src/*")
    list(APPEND files ${found})
endforeach()
list(SORT files)

set(inputs "toolchain ${TOOLCHAIN}\n")
foreach(f IN LISTS files)
    file(RELATIVE_PATH name "${SOURCE_DIR}" "${f}")
    file(SHA256 "${f}" digest)
    string(APPEND inputs "source ${name} ${digest}\n")
endforeach()
foreach(f IN LISTS library_files)
    get_filename_component(name "${f}" NAME)
    file(SHA256 "${f}" digest)
    string(APPEND inputs "library ${name} ${digest}\n")
endforeach()
string(SHA256 code_hash "${inputs}")

# The inputs beside the source, to tell why a hash changed. Nothing reads it.
get_filename_component(output_dir "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${output_dir}")
file(WRITE "${OUTPUT_FILE}.inputs" "${inputs}")
file(WRITE "${OUTPUT_FILE}.tmp"
"#include \"${HEADER}\"

namespace ${NAMESPACE} {

std::string_view code_hash() noexcept {
    return \"${code_hash}\";
}

}
")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${OUTPUT_FILE}.tmp" "${OUTPUT_FILE}")
file(REMOVE "${OUTPUT_FILE}.tmp")
