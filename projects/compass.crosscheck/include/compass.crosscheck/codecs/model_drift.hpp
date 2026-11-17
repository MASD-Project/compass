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
#ifndef COMPASS_CROSSCHECK_CODECS_MODEL_DRIFT_HPP
#define COMPASS_CROSSCHECK_CODECS_MODEL_DRIFT_HPP

#include "compass.codec/types/descriptor.hpp"

namespace compass::crosscheck::codecs {

/**
 * @brief ORE Studio's model-drift gate (check_model_drift.py): every
 * model matches the profiles it binds to.
 *
 * A profile is projects/modeling/variability_<slug>.org of #+type: profile;
 * its * Assignments table fixes feature values. A model directly under
 * projects/<component>/modeling/ binds by its profile property, read with
 * every other property of every drawer. Rules: contradicted, unset_required
 * for a feature fixed as required, self_contradicting for a feature stated
 * with two values, and no_profiles. An unknown profile and two profiles
 * that disagree are not reported: codegen's model loader refuses both. The script's KNOWN_MODEL_DRIFT entry is a finding, excused
 * by an inline exception in the model.
 */
extern const compass::codec::types::descriptor model_drift;

}

#endif
