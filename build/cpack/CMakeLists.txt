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

# CMake >= 4.4 enforces our warnings.uninitialized preset (see
# CMakePresets.json) inside CMake's own bundled
# Modules/GNUInstallDirs.cmake, which dereferences each CMAKE_INSTALL_<dir>
# cache entry before it creates it, once per directory. That module isn't
# ours to patch, so silence uninitialized warnings only around this include.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
    cmake_diagnostic(PUSH)
    cmake_diagnostic(SET CMD_UNINITIALIZED IGNORE)
endif()
include(GNUInstallDirs)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
    cmake_diagnostic(POP)
endif()

# CMake >= 4.4 enforces our warnings.uninitialized preset (see
# CMakePresets.json) inside CMake's own bundled
# Modules/InstallRequiredSystemLibraries.cmake, which reads
# CMAKE_C_COMPILER_ID/CMAKE_Fortran_COMPILER_ID before anything sets them.
# That module isn't ours to patch, so silence uninitialized warnings only
# around this include.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
    cmake_diagnostic(PUSH)
    cmake_diagnostic(SET CMD_UNINITIALIZED IGNORE)
endif()
include(InstallRequiredSystemLibraries)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
    cmake_diagnostic(POP)
endif()

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "compass - ${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_CONTACT "https://github.com/MASD-Project/compass")
set(CPACK_PACKAGE_VENDOR "MASD Project")

set(CPACK_PACKAGE_INSTALL_DIRECTORY ${CPACK_PACKAGE_NAME})
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/packages")
set(CPACK_STRIP_FILES YES)

set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)

set(CPACK_PACKAGE_CONTACT "marco.craveiro@gmail.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Marco Craveiro <${CPACK_PACKAGE_CONTACT}>")

#
# Component configuration
#
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/readme.org")

# This is for WiX so that it does not complain about unsupported extension
# of license file.
configure_file("${CMAKE_SOURCE_DIR}/LICENSE"
    "${CMAKE_BINARY_DIR}/LICENSE.TXT" COPYONLY)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_BINARY_DIR}/LICENSE.TXT")

#
# platform specific
#
if(WIN32)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")

    if (MSVC)
        # don't forget to set the WIX path, e.g.:
        # WIX="C:/Program Files (x86)/WiX Toolset v3.10/bin" cpack -G WIX -C Release
        set(CPACK_GENERATOR "WIX")
        set(CPACK_PACKAGE_EXECUTABLES "compass" "compass")
        set(CPACK_WIX_PRODUCT_GUID "5655AA29-AD2C-4B52-B27E-07413BF08F1A")
        set(CPACK_WIX_UPGRADE_GUID "05F1FAC2-1BDB-44F6-BC2E-DB2F51AC2A6F")
    else()
        set(CPACK_GENERATOR "NSIS")
        set(CPACK_PACKAGE_EXECUTABLES "compass" "compass")

        if(CMAKE_CL_64)
            set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
        else()
            set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES")
        endif()

        set(CPACK_NSIS_DISPLAY_NAME "compass")
        set(CPACK_NSIS_PACKAGE_NAME "compass ${PROJECT_VERSION}")
        set(CPACK_NSIS_HELP_LINK "https://github.com/MASD-Project/compass")
        set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/MASD-Project/compass")
        set(CPACK_NSIS_CONTACT "")
        set(CPACK_NSIS_MODIFY_PATH ON)
    endif()
elseif(APPLE)
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
    # compass is a command-line tool with no application bundle, so the disk
    # image holds the install tree as it is.
    set(CPACK_GENERATOR "DragNDrop")
elseif(UNIX)
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_STRIP_FILES "")
    set(CPACK_SOURCE_STRIP_FILES "")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
    set(CPACK_DEB_COMPONENT_INSTALL YES)
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)

    set(CPACK_DEB_PACKAGE_DEBUG "1")
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Marco Craveiro")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "extra")
    set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "")
else()
    message(FATAL_ERROR "unknown operating system")
endif()

# CMake >= 4.4 enforces our warnings.uninitialized preset inside CMake's own
# bundled Modules/CPack.cmake, which reads CPACK_SOURCE_GENERATOR/
# CPACK_SOURCE_INSTALL_CMAKE_PROJECTS before anything sets them. That module
# isn't ours to patch, so silence uninitialized warnings only around this
# include.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
    cmake_diagnostic(PUSH)
    cmake_diagnostic(SET CMD_UNINITIALIZED IGNORE)
endif()
include(CPack)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.4")
    cmake_diagnostic(POP)
endif()
