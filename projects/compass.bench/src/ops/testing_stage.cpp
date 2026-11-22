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
#include "compass.bench/ops/testing_stage.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include "compass.db/io/connection.hpp"
#include "compass.db/query/testing.hpp"

namespace compass::bench::ops {

namespace {

double median(std::vector<double> v) {
    if (v.empty())
        return 0;
    std::ranges::sort(v);
    return v[v.size() / 2];
}

template <typename F>
double timed(F&& f) {
    const auto start = std::chrono::steady_clock::now();
    f();
    return std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
}

}

testing_result run_testing_stage(const std::string& conninfo,
    const db::types::sprint_request& r, unsigned repetitions) {
    db::io::connection c(conninfo);
    const db::types::view_filter team{"true"};
    testing_result out;
    out.testing_first_ms = timed([&] {
        out.testing = db::query::what_needs_testing(c, team, r);
    });
    for (unsigned i = 0; i < repetitions; ++i)
        out.testing_warm_ms.push_back(timed([&] {
            static_cast<void>(db::query::what_needs_testing(c, team, r));
        }));
    out.sprint_first_ms = timed([&] {
        out.sprint = db::query::sprint_state(c, team, r);
    });
    for (unsigned i = 0; i < repetitions; ++i)
        out.sprint_warm_ms.push_back(timed([&] {
            static_cast<void>(db::query::sprint_state(c, team, r));
        }));
    return out;
}

void write_testing_report(std::ostream& os, const testing_result& result) {
    using db::types::scenario_state;
    const auto& t = result.testing;
    os << std::format("sprint {} ({}) {}, synced {}\n", t.sprint.title,
        t.sprint.org_id, t.sprint.state, t.synced_at);

    std::array<std::size_t, 4> counts{};
    std::size_t scenarios = 0;
    std::size_t with_scenarios = 0;
    std::uint64_t notes = 0;
    for (const auto& s : t.stories) {
        with_scenarios += s.scenarios.empty() ? 0 : 1;
        notes += s.open_notes;
        for (const auto& sc : s.scenarios) {
            ++counts[static_cast<std::size_t>(sc.state)];
            ++scenarios;
        }
    }
    os << std::format("stories {}, with scenarios {}, scenarios {}, open "
        "notes {}\n", t.stories.size(), with_scenarios, scenarios, notes);
    for (const auto s : {scenario_state::untested, scenario_state::failed,
             scenario_state::passed, scenario_state::changed})
        os << std::format("  {:<9} {}\n", db::types::to_string(s),
            counts[static_cast<std::size_t>(s)]);

    os << "\nscenarios by story\n";
    for (const auto& s : t.stories) {
        if (s.scenarios.empty())
            continue;
        os << std::format("  {} [{}] campaign {} open notes {}\n", s.title,
            s.state, s.campaign ? std::to_string(*s.campaign) : "-",
            s.open_notes);
        for (const auto& sc : s.scenarios) {
            os << std::format("    {:<9} notes {} {}", db::types::to_string(
                sc.state), sc.open_notes, sc.title);
            if (sc.latest_run)
                os << std::format(" (run {} by {}: {})", sc.latest_run->id,
                    sc.latest_run->tester, sc.latest_run->outcome);
            os << "\n";
        }
    }

    std::size_t tasks = 0;
    std::vector<std::pair<std::string, std::size_t>> task_states;
    for (const auto& s : result.sprint.stories)
        for (const auto& task : s.tasks) {
            ++tasks;
            const auto state = task.state.empty() ? "(none)" : task.state;
            const auto at = std::ranges::find(task_states, state,
                &std::pair<std::string, std::size_t>::first);
            if (at == task_states.end())
                task_states.emplace_back(state, 1);
            else
                ++at->second;
        }
    std::ranges::sort(task_states);
    os << std::format("\nsprint state: stories {}, tasks {}\n",
        result.sprint.stories.size(), tasks);
    for (const auto& [state, n] : task_states)
        os << std::format("  tasks {:<10} {}\n", state, n);

    os << std::format("\nlatency what needs testing: first {:.1f} ms, warm "
        "median {:.1f} ms over {}\n", result.testing_first_ms,
        median(result.testing_warm_ms), result.testing_warm_ms.size());
    os << std::format("latency sprint state:       first {:.1f} ms, warm "
        "median {:.1f} ms over {}\n", result.sprint_first_ms,
        median(result.sprint_warm_ms), result.sprint_warm_ms.size());
}

}
