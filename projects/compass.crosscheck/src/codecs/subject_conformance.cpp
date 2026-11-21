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
#include "compass.crosscheck/codecs/subject_conformance.hpp"

#include <array>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.crosscheck/ops/subjects.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::uint32_t outside_protocol = 0;

constexpr std::array<ct::rule, 1> rules{{
    {"outside_protocol", "Every NATS subject the data-quality seed spells "
        "is inside the entity protocol.", {}, ct::blame_method::line}}};

constexpr std::string_view dq =
    "projects/ores.sql/populate/dq/dq_artefact_types_populate.sql";

std::vector<std::string_view> scanned(const ct::corpus_view& v) {
    if (v.file_exists(dq))
        return {dq};
    return {};
}

/**
 * @brief Each quoted dotted name of the seed, once per line, that is not
 * inside the protocol.
 */
void map(std::string_view path, std::string_view bytes,
    const ct::option_values&, ct::file_scan& out) {
    static const pt::regex seeded(
        R"(\'([a-z_]+\.[a-z0-9_]+\.[a-z0-9_.]+)\')");
    if (bytes.find('\'') == std::string_view::npos)
        return;
    const auto text = pt::universal_newlines(pt::decode_replacing(bytes));
    std::set<std::pair<std::uint32_t, std::string>> seen;
    for (const auto& m : seeded.find_all(text)) {
        std::string subject(m.group(1));
        while (subject.ends_with('.'))
            subject.pop_back();
        if (subject.empty())
            continue;
        const auto line = co::line_of(text, m.start());
        if (!seen.emplace(line, subject).second)
            continue;
        const auto [ok, reason] = ops::classify_subject(subject);
        if (!ok)
            co::report(out.findings, path, co::offset_of_line(bytes, line),
                outside_protocol, subject + ": " + reason,
                ops::subject_fix);
    }
}

}

const ct::descriptor subject_conformance{
    .name = "subject_conformance",
    .version = 4,
    .summary = "Reports each NATS subject the data-quality seed spells "
        "outside the entity protocol.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::scan_function{scanned, map, nullptr}};

}
