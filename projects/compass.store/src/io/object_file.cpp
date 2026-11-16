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
#include "compass.store/io/object_file.hpp"

#include <cstdint>
#include <format>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

namespace compass::store::io {

namespace {

/**
 * @brief A name no other writer picks: 64 random bits, drawn from a
 * generator each thread seeds from the system once.
 */
std::string temporary_name() {
    thread_local std::mt19937_64 generator{[] {
        std::random_device device;
        return (std::uint64_t{device()} << 32) | device();
    }()};
    return std::format("tmp_{:016x}", generator());
}

}

void write_object(const std::filesystem::path& target,
    std::span<const std::byte> bytes) {
    const auto directory = target.parent_path();
    std::filesystem::create_directories(directory);
    const auto temporary = directory / temporary_name();

    std::ofstream os(temporary, std::ios::binary | std::ios::noreplace);
    os.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    os.close();
    std::error_code error;
    if (!os)
        error = std::make_error_code(std::errc::io_error);
    else
        std::filesystem::rename(temporary, target, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::filesystem::filesystem_error("cannot write object",
            target, error);
    }
}

std::optional<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream is(path, std::ios::binary | std::ios::ate);
    if (!is)
        return std::nullopt;
    const auto size = is.tellg();
    if (size < 0)
        return std::nullopt;
    std::string bytes(static_cast<std::size_t>(size), '\0');
    is.seekg(0);
    if (!is.read(bytes.data(), size))
        return std::nullopt;
    return bytes;
}

}
