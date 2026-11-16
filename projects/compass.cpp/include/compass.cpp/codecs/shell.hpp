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
#ifndef COMPASS_CPP_CODECS_SHELL_HPP
#define COMPASS_CPP_CODECS_SHELL_HPP

#include "compass.codec/types/descriptor.hpp"

/**
 * @brief ORE Studio's registrar and shell composition gates, which each read
 * a handful of files of named components.
 */
namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's shell-command-units gate over the .cpp files of
 * projects/ores.shell outside tests. Rule menu_built_twice: a file builds
 * one menu twice. Rule menu_owned_twice: a menu another file already
 * builds. Rule unit_registered_twice: a line registers a unit a second
 * time in one file.
 */
extern const compass::codec::types::descriptor shell_command_units;

/**
 * @brief ORE Studio's shell-menus gate over the .cpp files of
 * projects/ores.shell. Rule menu_with_prompt: a cli::Menu built with a
 * second argument. Rule root_name_collides: two menus or root commands
 * own one name, reported on the second.
 */
extern const compass::codec::types::descriptor shell_menus;

/**
 * @brief ORE Studio's shell-command-composition gate: every
 * generated command unit of refdata and reporting is registered by a .cpp
 * under projects/ores.shell/application/src. Rule unregistered_unit: a
 * unit header marked AUTO-GENERATED FILE whose register_commands nothing
 * calls.
 */
extern const compass::codec::types::descriptor shell_command_composition;

}

#endif
