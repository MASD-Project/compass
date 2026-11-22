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

# The MASD library ships with compass. A codegen library option of @compass
# resolves to share/compass/masd beside the directory of the running binary,
# so the package installs it there and every build stages it at the same
# place beside publish/bin. Staging copies afresh, so a file deleted here is
# gone from the build tree too.
set(masd_library_source ${CMAKE_CURRENT_SOURCE_DIR}/masd)
set(masd_library_staged ${COMPASS_PUBLISH_DIRECTORY}/share/compass/masd)

add_custom_target(compass.masd_library ALL
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${masd_library_staged}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${masd_library_source}"
        "${masd_library_staged}"
    COMMENT "Staging the MASD library beside the compass binary"
    VERBATIM)

# The codegen tests resolve @compass against their own binary in
# publish/bin.
if(TARGET compass.codegen.tests)
    add_dependencies(compass.codegen.tests compass.masd_library)
endif()

install(DIRECTORY ${masd_library_source}/ DESTINATION share/compass/masd)
