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
#ifndef COMPASS_PLATFORM_PROCESS_STANDARD_STREAMS_HPP
#define COMPASS_PLATFORM_PROCESS_STANDARD_STREAMS_HPP

namespace compass::platform::process {

/**
 * @brief Makes standard input, output and error carry bytes unchanged, so
 * a program prints the same bytes on every system. Windows otherwise reads
 * each CRLF as LF and writes each LF as CRLF. Call it before any I/O.
 */
void use_binary_standard_streams() noexcept;

}

#endif
