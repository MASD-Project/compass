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
#ifndef COMPASS_CPP_CODECS_TEST_CASE_REACHABILITY_HPP
#define COMPASS_CPP_CODECS_TEST_CASE_REACHABILITY_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's reachability gate: no test case sits inside a
 * conditional compilation block.
 *
 * Reads every test source under projects/ (ops::is_test_source). Rule
 * case_in_conditional: a Catch2 case is declared inside #if, #ifdef or
 * #ifndef, so the build can compile it away while the suite stays green.
 * Rule unbalanced_conditional: an #endif closes nothing, or a block is
 * still open at the end of the file. Rule raw_string_moves: removing the
 * backslash-newline splices moves a raw string, so the file's
 * conditionals cannot be read and its cases are left out. Rule
 * no_test_case: no test source, or no case in any readable one.
 */
extern const compass::codec::types::descriptor test_case_reachability;

}

#endif
