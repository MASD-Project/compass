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
#ifndef COMPASS_DOCS_CODECS_SKILL_TABLES_HPP
#define COMPASS_DOCS_CODECS_SKILL_TABLES_HPP

#include <span>
#include <string_view>
#include <utility>
#include "compass.codec/types/descriptor.hpp"

namespace compass::docs::codecs {

/**
 * @brief ORE Studio's skill-catalogue generator
 * (generate_skills_catalogue.py): the generated region of
 * doc/llm/skills/claude_code_skills.org.
 *
 * Each skill directory under doc/llm/skills with a SKILL.org is grouped by
 * the segment after compass-, in the script's domain order, as a row of
 * its ID, name, title and description, the description cut to 110
 * characters. The first region between the script's BEGIN and END markers
 * is replaced; a catalogue without them is not written.
 */
extern const compass::codec::types::descriptor skill_catalogue;

/**
 * @brief ORE Studio's skill-type-tables generator
 * (generate_skill_type_tables.py): the document types table of
 * compass-doc-add and the read-only commands table of compass-doc-show.
 *
 * The types come from the doc_*.org.mustache templates and the parents
 * compass.py's _STATIC_PARENT and _DEFAULTABLE_PARENT name; the commands
 * from the help of compass.py's subparsers. Every region between a
 * table's markers is replaced.
 */
extern const compass::codec::types::descriptor skill_type_tables;

/**
 * @brief The inventory half of ORE Studio's skill-levels generator
 * (apply_skill_levels.py): the generated region of
 * doc/llm/skills/skill_systems_levels.org, the count of skills each
 * System sees by the script's table. A skill missing from the table, or a
 * table entry with no skill, writes nothing, where the script stops; an
 * inventory without its markers is not written.
 */
extern const compass::codec::types::descriptor skill_levels;

/**
 * @brief The page half of apply_skill_levels.py, as the verdict its
 * --check gives: each skill whose #+level: is not the table's, each skill
 * the table lacks and each table entry with no skill.
 *
 * The script writes the level into the page. That page is also written by
 * structure_notes, and an output has one producer, so this half reports
 * instead of writing.
 */
extern const compass::codec::types::descriptor skill_level_tags;

/**
 * @brief The table skill_levels applies: each skill and the System it
 * serves, sorted by skill.
 */
[[nodiscard]] std::span<const std::pair<std::string_view, std::string_view>>
skill_level_table() noexcept;

}

#endif
