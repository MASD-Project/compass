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
#ifndef COMPASS_CROSSCHECK_CODECS_TEMPLATE_SECTIONS_HPP
#define COMPASS_CROSSCHECK_CODECS_TEMPLATE_SECTIONS_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's template-sections gate (check_template_sections.py):
 * a Mustache line holds one section tag, so the renderer can strip it.
 *
 * Reads each .mustache directly in the templates directory. Rule
 * chained_tags: a line holding two or more section tags and nothing else,
 * one of which opens or closes a section that spans lines. Each line is a
 * finding; the script's per-template counts are replaced by inline
 * exceptions.
 */
extern const compass::codec::types::descriptor template_sections;

}

#endif
