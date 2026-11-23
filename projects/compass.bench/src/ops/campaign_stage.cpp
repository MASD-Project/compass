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
#include "compass.bench/ops/campaign_stage.hpp"

#include <chrono>
#include <cstdint>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include "compass.db/io/connection.hpp"
#include "compass.db/query/campaign.hpp"
#include "compass.db/types/step_result_row.hpp"

namespace compass::bench::ops {

namespace {

using db::types::step_outcome;

std::string rest_of(std::istringstream& in) {
    std::string rest;
    std::getline(in >> std::ws, rest);
    return rest;
}

std::optional<std::string> optional_rest(std::istringstream& in) {
    auto rest = rest_of(in);
    if (rest.empty())
        return std::nullopt;
    return rest;
}

std::string tester_of(const std::string& word) {
    return word == "-" ? std::string() : word;
}

step_outcome outcome_of(const std::string& word) {
    if (const auto o = db::types::step_outcome_of(word))
        return *o;
    throw std::runtime_error("no step outcome " + word);
}

/**
 * @brief The image type of @p file, by its extension: JPEG, GIF or WebP,
 * else PNG.
 */
std::string media_type_of(const std::string& file) {
    const auto ext = std::filesystem::path(file).extension().string();
    if (ext == ".jpg" || ext == ".jpeg")
        return "image/jpeg";
    if (ext == ".gif")
        return "image/gif";
    if (ext == ".webp")
        return "image/webp";
    return "image/png";
}

std::string read_file(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in)
        throw std::runtime_error("cannot read " + p.string());
    return {std::istreambuf_iterator<char>(in), {}};
}

struct session {
    db::io::connection& c;
    const std::string& environment;
    const db::types::note_kinds& kinds;
    const std::filesystem::path& base;
    std::int64_t run = 0;
    std::int64_t campaign = 0;
    std::int64_t note = 0;
    std::string tester;

    std::string play(const std::string& verb, std::istringstream& in) {
        if (verb == "start") {
            db::types::start_run_request r;
            r.environment = environment;
            std::string who;
            in >> r.scenario_id >> who;
            r.tester = tester_of(who);
            const auto out = db::query::start_run(c, r);
            run = out.run;
            campaign = out.campaign;
            tester = r.tester;
            return std::format("run {} campaign {} steps {} definition {}",
                out.run, out.campaign, out.steps.size(),
                out.definition.substr(0, 10));
        }
        if (verb == "campaign") {
            std::string who;
            in >> campaign >> who;
            tester = tester_of(who);
            return std::format("campaign {}", campaign);
        }
        if (verb == "step") {
            db::types::step_result_request r;
            std::string outcome;
            in >> r.step_ord >> outcome;
            r.run = run;
            r.outcome = outcome_of(outcome);
            r.notes = optional_rest(in);
            r.tester = tester;
            db::query::record_step_result(c, r);
            return "recorded";
        }
        if (verb == "attach") {
            db::types::attach_request r;
            std::string file;
            std::string where;
            in >> file >> where;
            if (where == "note") {
                r.note = note;
            } else {
                r.run = run;
                if (where != "run")
                    r.step_ord = std::stoi(where);
            }
            r.media_type = media_type_of(file);
            r.bytes = read_file(base / file);
            r.caption = optional_rest(in);
            r.tester = tester;
            const auto out = db::query::attach_screenshot(c, r);
            return std::format("attached {} as {}", out.attached,
                out.attachment.substr(0, 10));
        }
        if (verb == "note") {
            db::types::add_note_request r;
            std::string scenario;
            std::string step;
            in >> r.kind >> scenario >> step;
            r.campaign = campaign;
            if (scenario != "-")
                r.scenario_id = scenario;
            if (step != "-")
                r.step_ord = std::stoi(step);
            r.text = rest_of(in);
            r.tester = tester;
            note = db::query::add_note(c, kinds, r);
            return std::format("note {}", note);
        }
        if (verb == "address") {
            db::types::address_note_request r;
            std::string who;
            in >> who;
            r.note = note;
            r.tester = tester_of(who);
            r.reply = optional_rest(in);
            db::query::address_note(c, r);
            return "addressed";
        }
        if (verb == "finish") {
            db::types::finish_run_request r;
            r.run = run;
            r.tester = tester;
            const auto out = db::query::finish_run(c, r);
            return std::format("outcome {}", db::types::to_string(
                out.outcome));
        }
        throw std::runtime_error("no script verb " + verb);
    }
};

std::string_view reason_of(db::query::rejected_write::reason r) {
    using enum db::query::rejected_write::reason;
    switch (r) {
    case invalid: return "invalid";
    case not_found: return "not found";
    case conflict: return "conflict";
    }
    return "invalid";
}

}

void run_campaign_stage(const std::string& conninfo,
    const std::string& environment, const db::types::note_kinds& kinds,
    std::istream& script, const std::filesystem::path& base,
    std::ostream& os) {
    db::io::connection c(conninfo);
    session s{c, environment, kinds, base};
    for (std::string line; std::getline(script, line);) {
        if (line.empty() || line.front() == '#')
            continue;
        std::istringstream in(line);
        std::string verb;
        in >> verb;
        const auto start = std::chrono::steady_clock::now();
        std::string result;
        try {
            result = s.play(verb, in);
        } catch (const db::query::rejected_write& e) {
            result = std::format("REJECTED ({}): {}", reason_of(e.why()),
                e.what());
        }
        const auto ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        os << std::format("{:<60} -> {} [{:.1f} ms]\n", line.substr(0, 60),
            result, ms);
    }
}

}
