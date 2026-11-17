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
#ifndef COMPASS_CPP_OPS_TEST_CASE_REACHABILITY_HPP
#define COMPASS_CPP_OPS_TEST_CASE_REACHABILITY_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief The rules of ORE Studio's check_test_case_reachability.py over
 * the text of one test source.
 */
namespace compass::cpp::ops {

/**
 * @brief True when the script reads @p path, relative to the checkout:
 * under projects/, not under a node_modules, venv, .venv, build or
 * external directory, with a C++ source or fragment suffix, in a tests
 * directory or named *_tests.
 */
[[nodiscard]] bool is_test_source(std::string_view path);

/**
 * @brief A test case declared inside a conditional block: the 1-based
 * line of the case and its text, and the line and text of the directive
 * that opened the innermost block. Texts are stripped lines of the
 * source.
 */
struct conditional_case {
    std::uint32_t line = 0;
    std::string text;
    std::uint32_t opened_line = 0;
    std::string opened_text;
};

/**
 * @brief Why the conditionals of a file cannot be certified.
 */
enum class reading_problem : std::uint8_t {
    /** An #endif closes a conditional that nothing opened. */
    unopened_endif,
    /** A conditional is still open at the end of the file. */
    unclosed_conditional,
    /** Removing the splices moves where a raw string begins or ends. */
    raw_string_moves
};

struct problem {
    std::uint32_t line = 0;
    std::string text;
    reading_problem kind = reading_problem::unopened_endif;
};

/**
 * @brief What scanning one test source found: how many cases it declares,
 * each case inside a conditional, one per declaration, and each problem.
 */
struct scan_result {
    std::uint64_t declared = 0;
    std::vector<conditional_case> cases;
    std::vector<problem> problems;
};

/**
 * @brief Scans @p text as the script does: splices removed, comments and
 * literals blanked by ops::lex, #if, #ifdef and #ifndef opening a block
 * and #endif closing one, and a Catch2 test macro, or a name the file's own #define
 * binds to one, followed by "(" declaring a case.
 */
[[nodiscard]] scan_result scan_source(std::string_view text);

}

#endif
