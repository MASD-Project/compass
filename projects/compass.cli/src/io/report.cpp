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
#include <sstream>
#include <string>
#include <vector>
#include "compass.store/ops/blob_hash.hpp"

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

std::string excused_note(std::size_t excused) {
    return excused == 0 ? std::string() : std::format(", {} excused by "
        "exception{}", excused, excused == 1 ? "" : "s");
}

void write_findings(std::ostream& out,
    const std::vector<types::finding>& findings) {
    for (const auto& f : findings)
        out << std::format("{}:{}:{}: {}: {} (fix: {})\n", f.path, f.line,
            f.column, f.rule, f.message, f.fix);
}

/**
 * @brief @p text as a JSON string, quotes included.
 */
std::string json_string(std::string_view text) {
    std::string out = "\"";
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
                out += std::format("\\u{:04x}",
                    static_cast<unsigned>(static_cast<unsigned char>(c)));
            else
                out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
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

void write_hook(std::ostream& out, std::ostream& err,
    const types::hook_result& r) {
    write_problems(err, r.run);
    if (!r.note.empty()) {
        out << std::format("{{\"hookSpecificOutput\": {{\"hookEventName\": "
            "\"PostToolUse\", \"additionalContext\": {}}}}}\n",
            json_string("compass hook: " + r.note));
        return;
    }
    const auto old = r.old == 0 ? std::string() : std::format("{} finding{} "
        "the last check recorded on {} {} not shown; compass check lists "
        "them", r.old, r.old == 1 ? "" : "s", r.path,
        r.old == 1 ? "is" : "are");
    if (r.findings.empty()) {
        if (!old.empty())
            out << std::format("{{\"hookSpecificOutput\": {{\"hookEventName\": "
                "\"PostToolUse\", \"additionalContext\": {}}}}}\n",
                json_string("compass hook: no new findings; " + old));
        return;
    }
    std::ostringstream lines;
    write_findings(lines, r.findings);
    const auto count = r.findings.size();
    const auto reason = std::format("compass found {} finding{} on or "
        "caused by the edit to {}{}{}:\n{}", count, count == 1 ? "" : "s",
        r.path, excused_note(r.excused), old.empty() ? "" : "; " + old,
        lines.str());
    out << std::format("{{\"decision\": \"block\", \"reason\": {}}}\n",
        json_string(reason));
}

void write_problems(std::ostream& err, const types::run_summary& s) {
    write_lines(err, "", s.errors);
    write_lines(err, "codec registry: ", s.problems);
}

void write_generate(std::ostream& out, std::ostream& err,
    const types::generate_result& r) {
    write_problems(err, r.run);
    write_lines(err, "output: ", r.sink.problems);
    for (const auto& h : r.hand_edits)
        err << std::format("compass: warning: {}:{}: generate deleted {} "
                           "line{} its output lacks, from \"{}\"; if they "
                           "were written by hand, restore them from git "
                           "and move them into a template or a hand-written "
                           "file\n", h.path, h.block.line, h.block.lines,
            h.block.lines == 1 ? "" : "s", h.block.first);
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
    write_findings(out, r.findings);
    out << std::format("{} diagnostic{}{} ({})\n", r.findings.size(),
        r.findings.size() == 1 ? "" : "s", excused_note(r.excused),
        corpus_note(r.run, r.ms));
}

void write_ratchet(std::ostream& out, std::ostream& err,
    const types::ratchet_result& r) {
    write_problems(err, r.current.run);
    write_lines(err, "merge base: ", r.previous.run.errors);
    write_lines(err, "merge base: codec registry: ", r.previous.run.problems);
    write_findings(out, r.split.fresh);
    const auto n = r.split.fresh.size();
    out << std::format("{} new diagnostic{}{}, {} excused as present at "
                       "{}, the merge base of HEAD and {}; {} renamed file{} "
                       "followed ({}; {:.1f} ms in all)\n", n,
        n == 1 ? "" : "s", excused_note(r.current.excused), r.split.excused,
        store::ops::to_hex(r.merge_base).substr(0, 12), r.base,
        r.renames.size(), r.renames.size() == 1 ? "" : "s",
        corpus_note(r.current.run, r.current.ms), r.ms);
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
        if (s.ran[c] != 0)
            err << std::format("; {:.1f} ms running", s.codec_ms[c]);
        err << "\n";
    }
}

void write_gc(std::ostream& out, const types::gc_result& r) {
    const auto& c = r.collection;
    const auto& plan = c.plan;
    const auto ms = c.worktrees_ms + c.live_ms + c.list_ms + c.plan_ms +
        c.remove_ms;
    const auto dropped = plan.victims.objects + c.superseded.objects;
    const auto dropped_bytes = plan.victims.bytes + c.superseded.bytes;
    if (r.options.dry_run) {
        out << std::format("would remove {} entries and files, {} bytes; "
                           "store {} bytes in {} entries, {} packs",
            dropped, dropped_bytes, plan.stored.bytes + c.superseded.bytes,
            plan.stored.objects + c.superseded.objects, c.packs);
        if (c.legacy.objects != 0)
            out << std::format("; would remove the old layout, {} files, {} "
                               "bytes", c.legacy.objects, c.legacy.bytes);
        out << std::format(" ({:.1f} ms)\n", ms);
        return;
    }
    out << std::format("removed {} entries and files, {} bytes; store {} "
                       "bytes in {} entries, {} packs (was {})",
        c.removed.objects, c.removed.bytes,
        plan.stored.bytes + c.superseded.bytes - c.removed.bytes,
        plan.stored.objects + c.superseded.objects - c.removed.objects,
        c.packs_after, c.packs);
    if (c.legacy.objects != 0)
        out << std::format("; removed the old layout, {} files, {} bytes",
            c.legacy.objects, c.legacy.bytes);
    out << std::format(" ({:.1f} ms)\n", ms);
}

}
