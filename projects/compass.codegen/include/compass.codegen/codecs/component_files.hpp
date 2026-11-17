/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2026 Marco Craveiro <marco.craveiro@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#ifndef COMPASS_CODEGEN_CODECS_COMPONENT_FILES_HPP
#define COMPASS_CODEGEN_CODECS_COMPONENT_FILES_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::codegen::codecs {

/**
 * @brief ORE Studio's cmake-sources generator
 * (regenerate_cmake_component_files.py --all): the component_files.cmake
 * lists of every modelled component.
 *
 * A component is a projects/.../modeling/component_overview.org with a
 * #+full_name: line; of two with one name the later path counts. For each
 * of its src and tests directories that holds a CMakeLists.txt, the
 * library's cmake_component_files_<dir>.mustache is rendered as
 * ores.codegen renders it, with component.files the .cpp files under the
 * directory and component.headers the .hpp files under the component's
 * include directory, each sorted and relative to its directory.
 */
extern const compass::codec::types::descriptor component_files;

}

#endif
