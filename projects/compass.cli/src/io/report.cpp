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
#include "compass.cli/io/report.hpp"

#include <format>
#include <string>
#include <vector>

namespace compass::cli::io {

namespace {

void write_lines(std::ostream& err, std::string_view prefix,
    const std::vector<std::string>& lines) {
    for (const auto& line : lines)
        err << std::format("compass: {}{}\n", prefix, line);
}

std::string corpus_note(const types::run_summary& s, double ms) {
    std::string units;
    for (std::size_t c = 0; c < s.codecs.size(); ++c)
        units += std::format("{}{} {}", c == 0 ? "" : ", ", s.codecs[c],
            s.ran[c]);
    return std::format("{} documents, {} from the store, {} parsed; "
                       "units run: {}; {:.1f} ms", s.documents, s.loaded,
        s.parsed, units.empty() ? "none" : units, ms);
}

std::string source_of(const types::origin& o,
    const std::filesystem::path& file) {
    switch (o.kind) {
    case types::origin_kind::defaults: return "default";
    case types::origin_kind::file:
        return std::format("{}:{}", file.filename().string(), o.line);
    case types::origin_kind::command_line: return "command line";
    }
    return "";
}

std::string shown(const std::filesystem::path& p,
    const std::filesystem::path& checkout) {
    const auto relative = p.lexically_relative(checkout);
    return relative.empty() || *relative.begin() == ".."
        ? p.string() : relative.string();
}

}

void write_problems(std::ostream& err, const types::run_summary& s) {
    write_lines(err, "", s.errors);
    write_lines(err, "codec registry: ", s.problems);
}

void write_generate(std::ostream& out, std::ostream& err,
    const types::generate_result& r) {
    write_problems(err, r.run);
    write_lines(err, "output: ", r.sink.problems);
    const auto& s = r.sink;
    out << std::format("{} written, {} unchanged, {} restored, {} removed, "
                       "{} kept under {} ({})\n", s.written, s.unchanged,
        s.restored, s.removed, s.kept, r.root.string(),
        corpus_note(r.run, r.ms));
}

void write_diff(std::ostream& out, std::ostream& err,
    const types::diff_result& r) {
    out << r.diff;
    write_problems(err, r.run);
    write_lines(err, "output: ", r.planned.problems);
    const auto& p = r.planned;
    err << std::format("{} to write, {} unchanged, {} to restore, {} to "
                       "remove, {} to keep under {} ({})\n", p.written,
        p.unchanged, p.restored, p.removed, p.kept, r.root.string(),
        corpus_note(r.run, r.ms));
}

void write_check(std::ostream& out, std::ostream& err,
    const types::check_result& r) {
    write_problems(err, r.run);
    for (const auto& f : r.findings)
        out << std::format("{}:{}:{}: {}: {} (fix: {})\n", f.path, f.line,
            f.column, f.rule, f.message, f.fix);
    out << std::format("{} diagnostic{} ({})\n", r.findings.size(),
        r.findings.size() == 1 ? "" : "s", corpus_note(r.run, r.ms));
}

void write_configuration(std::ostream& out, const types::configuration& c,
    const std::filesystem::path& checkout) {
    const auto line = [&](std::string_view key, std::string_view value,
                          const types::origin& from) {
        out << std::format("{} = {} ({})\n", key, value,
            source_of(from, c.file));
    };
    line("output", shown(c.output.value, checkout), c.output.from);
    line("meta", shown(c.meta.value, checkout), c.meta.from);
    for (const auto& codec : c.codecs) {
        line(codec.name + ".enabled", codec.enabled.value,
            codec.enabled.from);
        for (std::size_t i = 0; i < codec.keys.size(); ++i)
            line(codec.name + "." + codec.keys[i], codec.options[i].value,
                codec.options[i].from);
    }
}

void write_timings(std::ostream& err, const types::run_summary& s) {
    for (const auto& [step, ms] : s.steps)
        err << std::format("compass: step {} {:.1f} ms\n", step, ms);
    for (std::size_t c = 0; c < s.codecs.size(); ++c) {
        err << std::format("compass: codec {}: {} run, {} from the record, "
                           "{} from the cache", s.codecs[c], s.ran[c],
            s.skipped[c], s.cached[c]);
        if (const auto& f = s.formats[c]) {
            err << std::format("; format {} stored, {} formatted, {} "
                               "unformatted, {} processes, {:.1f} ms",
                f->hits, f->formatted, f->unformatted, f->runs,
                f->format_ms);
        }
        err << "\n";
    }
}

void write_gc(std::ostream& out, const types::gc_result& r) {
    const auto& c = r.collection;
    const auto& plan = c.plan;
    const auto ms = c.worktrees_ms + c.live_ms + c.list_ms + c.plan_ms +
        c.remove_ms;
    if (r.options.dry_run) {
        out << std::format("would remove {} files, {} bytes; store {} bytes "
                           "in {} files ({:.1f} ms)\n", plan.victims.objects,
            plan.victims.bytes, plan.stored.bytes, plan.stored.objects, ms);
        return;
    }
    out << std::format("removed {} files, {} bytes; store {} bytes in {} "
                       "files ({:.1f} ms)\n", c.removed.objects,
        c.removed.bytes, plan.stored.bytes - c.removed.bytes,
        plan.stored.objects - c.removed.objects, ms);
}

}
