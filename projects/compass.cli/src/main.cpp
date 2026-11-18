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
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>
#include "compass.cli/app/application.hpp"
#include "compass.platform/process/standard_streams.hpp"

int main(int argc, char** argv) {
    compass::platform::process::use_binary_standard_streams();
    const std::vector<std::string_view> args(argv + 1, argv + argc);
    std::error_code ec;
    const auto working_directory = std::filesystem::current_path(ec);
    if (ec) {
        std::cerr << "compass: cannot read the current directory: "
                  << ec.message() << "\n";
        return compass::cli::app::exit_failure;
    }
    return compass::cli::app::run(args, working_directory, std::cin,
        std::cout, std::cerr);
}
