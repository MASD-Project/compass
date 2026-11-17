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
#ifndef COMPASS_CPP_OPS_REPORT_HPP
#define COMPASS_CPP_OPS_REPORT_HPP

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include "compass.codec/types/descriptor.hpp"
#include "compass.codec/types/diagnostic_table.hpp"

namespace compass::cpp::ops {

/**
 * @brief Adds an error finding of rule @p rule to @p out, on byte
 * @p offset of the checkout file @p file.
 */
void report(compass::codec::types::diagnostic_table& out,
    std::string_view file, std::uint32_t offset, std::uint32_t rule,
    std::string_view message, std::string_view fix);

/**
 * @brief The one rule of a generator that refuses: rule 0 of its
 * descriptor.
 */
inline constexpr std::array<compass::codec::types::rule, 1> refusal_rules{{
    {"refused", "A generator writes its output only from inputs it can "
        "read and resolve; otherwise it says why and leaves the file it "
        "wrote before."}}};

/**
 * @brief Refuses to generate @p output, because of @p why: a finding of
 * refusal_rules on that file, whose message names it.
 */
void refuse(compass::codec::types::file_output& out, std::string_view output,
    std::string_view why, std::string_view fix);

/**
 * @brief @p problems joined by "; ", for the reason of a refusal.
 */
[[nodiscard]] std::string listed(std::span<const std::string> problems);

}

#endif
