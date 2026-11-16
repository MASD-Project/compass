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
#ifndef COMPASS_CPP_CODECS_REGISTRARS_HPP
#define COMPASS_CPP_CODECS_REGISTRARS_HPP

#include "compass.codec/types/descriptor.hpp"

/**
 * @brief ORE Studio's registrar and shell composition gates, which each read
 * a handful of files of named components.
 */
namespace compass::cpp::codecs {

/**
 * @brief ORE Studio's handler-registrars gate: every generated handler
 * registrar of a checked component has a responder. A registrar is a
 * header under core/include ending _registrar.hpp, not
 * _history_provider_registrar.hpp, that declares register_<entity>_handlers.
 * Rule registrar_without_responder: no source of the component outside
 * tests and the registrar's own files calls it, and the request subjects
 * its source subscribes are not all named elsewhere.
 */
extern const compass::codec::types::descriptor handler_registrars;

/**
 * @brief ORE Studio's event-registrars gate: every generated event
 * registrar of ores.dq, ores.refdata and ores.synthetic is called from
 * service/src/messaging/event_registrar.cpp. Rule uncomposed_registrar:
 * a registrar the file does not call. Rule missing_composition: the
 * component has registrars and no such file.
 */
extern const compass::codec::types::descriptor event_registrars;

/**
 * @brief ORE Studio's history-registrars gate: every generated
 * history-provider registrar of ores.refdata and ores.reporting is called
 * from core/src/messaging/registrar.cpp, with the same two rules as
 * event_registrars.
 */
extern const compass::codec::types::descriptor history_registrars;

}

#endif
