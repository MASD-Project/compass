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
#include "compass.cli/app/blame.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <system_error>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include "compass.cli/app/codecs.hpp"
#include "compass.cli/app/pipeline.hpp"
#include "compass.cli/io/history.hpp"
#include "compass.cli/io/listing.hpp"
#include "compass.cli/ops/blame.hpp"
#include "compass.cli/ops/provenance.hpp"
#include "compass.codec/ops/data_tables.hpp"
#include "compass.codec/ops/exceptions.hpp"
#include "compass.store/ops/blob_hash.hpp"

namespace compass::cli::app {

namespace {

namespace ct = compass::codec::types;
using ct::blame_method;

/**
 * @brief How many of main's last first-parent merges bisect walks when
 * no commit to start from is given.
 */
constexpr std::size_t merges_walked = 128;

bool same_name(std::string_view a, std::string_view b) {
    return std::ranges::equal(a, b, [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) ==
            std::tolower(static_cast<unsigned char>(y));
    });
}

bool uncommitted(std::string_view id) {
    return std::ranges::all_of(id, [](char c) { return c == '0'; });
}

/**
 * @brief The value of the option @p key of @p codec in @p o: the last
 * setting of it, else its default.
 */
std::string setting_of(const types::options& o, std::string_view codec,
    std::string_view key) {
    const auto& s = o.settings;
    for (auto i = s.size(); i-- > 0;)
        if (s.codecs[i] == codec && s.keys[i] == key)
            return s.values[i];
    const auto it = std::ranges::find(codecs(), codec,
        &ct::descriptor::name);
    if (it != codecs().end())
        for (const auto& option : it->options)
            if (option.key == key)
                return std::string(option.default_value);
    return {};
}

std::optional<std::string> read_text(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(in), {});
}

std::uint32_t line_count(std::string_view text) {
    const auto n = std::ranges::count(text, '\n');
    return static_cast<std::uint32_t>(n + (!text.empty() &&
        text.back() != '\n' ? 1 : 0));
}

using finding_key = std::tuple<std::string, std::string, std::string>;

finding_key key_of(const types::finding& f) {
    return {f.rule, f.path, f.message};
}

/**
 * @brief What blaming the findings of one run shares: the trailers to
 * read, main, and each commit read so far.
 */
struct tracer {
    const types::options& o;
    const types::document_ids& documents;
    const ct::dependency_record& record;
    std::vector<types::trailer_rule> trailers;
    std::string notify;
    std::string base;
    std::map<std::string, types::culprit> read;
    std::map<std::pair<std::string, std::string>, std::set<finding_key>>
        probed;
    std::size_t probes = 0;

    std::vector<std::string> git(std::vector<std::string> args) const {
        return io::git_lines(o.checkout, args);
    }

    /**
     * @brief The pull request of the first-parent merge of main that
     * brought the commit @p id, which main holds, into it: the oldest
     * merge of main's first parents that descends from it. None when
     * main's first parent held it before that merge, so it was made on
     * main.
     */
    std::optional<std::uint64_t> pull_request_of(const std::string& id) {
        const auto merges = git({"log", "--first-parent", "--merges",
            "--reverse", "--format=%H %s", id + ".." + base});
        const auto descendants = git({"rev-list", "--ancestry-path",
            "--merges", id + ".." + base});
        const std::set<std::string_view> holding(descendants.begin(),
            descendants.end());
        const auto merge = std::ranges::find_if(merges,
            [&](const std::string& m) {
                return holding.contains(std::string_view(m).substr(0,
                    m.find(' ')));
            });
        if (merge == merges.end())
            return std::nullopt;
        const auto space = merge->find(' ');
        if (space == std::string::npos || io::is_ancestor(o.checkout, id,
                merge->substr(0, space) + "^1"))
            return std::nullopt;
        return ops::pull_request_in(merge->substr(space + 1));
    }

    types::culprit culprit_of(const std::string& id,
        std::string label = {}) {
        types::culprit out;
        if (uncommitted(id)) {
            out.subject = "not committed yet";
            out.label = std::move(label);
            return out;
        }
        if (const auto it = read.find(id); it != read.end()) {
            out = it->second;
            out.label = std::move(label);
            return out;
        }
        const auto c = io::read_commit(o.checkout, id);
        out.id = c ? c->id : id;
        if (c) {
            out.subject = c->subject;
            out.author = c->author;
            for (const auto& t : trailers) {
                types::stated_trailer stated{t.trailer, {}, {}};
                const auto it = std::ranges::find_if(c->trailers,
                    [&](const auto& x) { return same_name(x.key, t.trailer); });
                if (it != c->trailers.end()) {
                    stated.value = it->value;
                    if (!t.type.empty())
                        if (const auto d = documents.find(it->value);
                            d != documents.end())
                            stated.title = d->second.title;
                }
                if (same_name(t.trailer, notify))
                    out.environment = stated.value;
                out.trailers.push_back(std::move(stated));
            }
            out.on_main = io::is_ancestor(o.checkout, c->id, base);
            out.pull_request = ops::pull_request_in(c->subject);
            if (!out.pull_request && out.on_main)
                out.pull_request = pull_request_of(c->id);
        }
        read.emplace(out.id, out);
        out.label = std::move(label);
        return out;
    }

    /**
     * @brief Traces the findings @p which of @p out, all on @p path, to
     * the commits that last changed their lines.
     */
    void blame_lines(const std::string& path, std::vector<std::size_t> which,
        std::vector<types::blamed_finding>& out) {
        const auto text = read_text(o.checkout / path);
        const auto lines = text ? line_count(*text) : 0;
        std::vector<std::string> args{"blame", "--porcelain"};
        std::set<std::uint32_t> asked;
        for (const auto i : which) {
            const auto line = out[i].found.line;
            if (line == 0 || line > lines || !asked.insert(line).second)
                continue;
            args.push_back("-L");
            args.push_back(std::format("{},{}", line, line));
        }
        args.push_back("--");
        args.push_back(path);
        const auto parsed = asked.empty()
            ? ops::blamed_lines{} : ops::parse_blame_porcelain(git(args));
        for (const auto i : which) {
            const auto it = parsed.commits.find(out[i].found.line);
            if (it == parsed.commits.end()) {
                out[i].note = text ? "git blame does not know the line"
                                   : "the file is not in the checkout";
                continue;
            }
            out[i].culprits.push_back(culprit_of(it->second));
        }
    }

    void blame_added(types::blamed_finding& b) {
        const auto& f = b.found;
        b.subject = f.subject.empty() ? f.path : f.subject;
        std::vector<std::string> found;
        if (!f.subject_text.empty())
            found = git({"log", "-1", "--format=%H", "-S" + f.subject_text,
                "HEAD", "--", b.subject});
        if (found.empty() && !f.subject_text.empty())
            b.note = "no commit ever changed '" + f.subject_text + "' in " +
                b.subject + ", so the requirement may be newer than the file";
        if (found.empty())
            found = git({"log", "-1", "--follow", "--diff-filter=A",
                "--format=%H", "HEAD", "--", b.subject});
        if (found.empty()) {
            b.note = "no commit added " + b.subject;
            return;
        }
        b.culprits.push_back(culprit_of(found.front()));
    }

    /**
     * @brief The output of the run at @p path, a path of the checkout, as
     * the row of its file in the record and the unit that produced it.
     */
    std::optional<std::pair<ct::file_id, std::uint32_t>> output_at(
        std::string_view path) const {
        for (std::uint32_t u = 0; u < record.size(); ++u) {
            const auto first = record.file_firsts[ct::record_unit_id{u}].value;
            const auto end = u + 1 < record.size()
                ? record.file_firsts[ct::record_unit_id{u + 1}].value
                : static_cast<std::uint32_t>(record.files.size());
            for (auto f = first; f < end; ++f) {
                const auto relative = (o.output_root / std::string(
                    record.files.paths[ct::file_id{f}])).lexically_relative(
                        o.checkout).generic_string();
                if (relative == path)
                    return std::pair{ct::file_id{f}, u};
            }
        }
        return std::nullopt;
    }

    /**
     * @brief The paths unit @p unit of the record read, and whether it read
     * a set of documents by type, which the record does not list by path.
     */
    std::pair<std::set<std::string, std::less<>>, bool> inputs_of(
        std::uint32_t unit) const {
        const auto first = record.input_firsts[ct::record_unit_id{unit}].value;
        const auto end = unit + 1 < record.size()
            ? record.input_firsts[ct::record_unit_id{unit + 1}].value
            : static_cast<std::uint32_t>(record.input_count());
        std::set<std::string, std::less<>> out;
        bool typed = false;
        for (auto i = first; i < end; ++i) {
            const ct::input_id id{i};
            const auto kind = record.input_kinds[id];
            if (kind == ct::input_kind::path || kind == ct::input_kind::file)
                out.emplace(record.input_keys[id]);
            typed = typed || kind == ct::input_kind::type_set;
        }
        return {std::move(out), typed};
    }

    void blame_drift(types::blamed_finding& b) {
        const auto& f = b.found;
        const auto output = output_at(f.path);
        const auto disk = read_text(o.checkout / f.path);
        if (!disk) {
            b.note = "the generated file is not in the checkout";
            return;
        }
        auto range = std::pair{f.line, f.line};
        if (output)
            range = ops::differing_lines(*disk,
                record.files.contents[output->first]);
        const auto lines = line_count(*disk);
        range.first = std::clamp<std::uint32_t>(range.first, 1,
            std::max<std::uint32_t>(lines, 1));
        range.second = std::clamp<std::uint32_t>(range.second, range.first,
            std::max<std::uint32_t>(lines, 1));
        const auto parsed = ops::parse_blame_porcelain(git({"blame",
            "--porcelain", "-L", std::format("{},{}", range.first,
                range.second), "--", f.path}));
        std::string newest;
        std::int64_t newest_time = -1;
        for (const auto& [id, time] : parsed.times)
            if (time > newest_time) {
                newest = id;
                newest_time = time;
            }
        b.subject = std::format("{}:{}-{}", f.path, range.first,
            range.second);
        if (!newest.empty())
            b.culprits.push_back(culprit_of(newest, "hand edit"));
        if (!output)
            return;
        const auto [inputs, typed] = inputs_of(output->second);
        if (inputs.empty() && !typed)
            return;
        const auto last = git({"log", "-1", "--format=%H", "HEAD", "--",
            f.path});
        if (last.empty())
            return;
        // One walk lists what each commit since changed; a pathspec per
        // input would make git match every input against every path.
        std::string commit;
        std::vector<std::string> touched;
        for (auto& line : git({"log", "--reverse", "--no-merges",
                 "--name-only", "--format=%H", last.front() + "..HEAD"})) {
            if (line.size() == 40 && std::ranges::all_of(line, [](char c) {
                    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
                })) {
                if (!touched.empty())
                    break;
                commit = std::move(line);
            } else if (inputs.contains(line) ||
                       (typed && line.ends_with(".org"))) {
                touched.push_back(std::move(line));
            }
        }
        if (touched.empty())
            return;
        auto c = culprit_of(commit, "input changed without regenerating");
        c.inputs = std::move(touched);
        b.culprits.push_back(std::move(c));
    }

    /**
     * @brief The findings of @p codec at the commit @p id: a check of the
     * commit with that codec alone, through the result cache.
     */
    const std::set<finding_key>& findings_at(const std::string& id,
        const std::string& codec) {
        const auto key = std::pair{id, codec};
        if (const auto it = probed.find(key); it != probed.end())
            return it->second;
        auto at = o;
        at.commit = io::resolve_commit(o.checkout, id);
        at.blame = types::blame_mode::off;
        for (const auto& d : codecs())
            at.settings.set(d.name, ct::enabled_key,
                d.name == codec ? "true" : "false");
        if (o.checks)
            for (const auto& k : o.checks->checks)
                at.settings.set(k->codec.name, ct::enabled_key,
                    k->codec.name == codec ? "true" : "false");
        const auto r = check(at);
        ++probes;
        std::set<finding_key> keys;
        for (const auto& f : r.findings)
            keys.insert(key_of(f));
        return probed.emplace(key, std::move(keys)).first->second;
    }

    /**
     * @brief The index of the first of @p commits that has @p f, given
     * that the last does and the commit before the first does not.
     */
    std::size_t first_with(const std::vector<std::string>& commits,
        const types::finding& f, const std::string& codec) {
        std::size_t lo = 0;
        std::size_t hi = commits.size() - 1;
        while (lo < hi) {
            const auto mid = lo + (hi - lo) / 2;
            if (findings_at(commits[mid], codec).contains(key_of(f)))
                hi = mid;
            else
                lo = mid + 1;
        }
        return lo;
    }

    void bisect(types::blamed_finding& b) {
        const auto& f = b.found;
        const auto codec = f.rule.substr(0, f.rule.find('/'));
        std::vector<std::string> chain;
        if (!o.bisect_from.empty()) {
            const auto from = git({"rev-parse", "--verify",
                o.bisect_from + "^{commit}"});
            if (from.empty()) {
                b.note = "--bisect-from " + o.bisect_from + " names no commit";
                return;
            }
            chain.push_back(from.front());
            std::ranges::move(git({"rev-list", "--first-parent", "--merges",
                "--reverse", from.front() + ".." + base}),
                std::back_inserter(chain));
        } else {
            chain = git({"rev-list", "--first-parent", "--merges", "-n",
                std::to_string(merges_walked), base});
            std::ranges::reverse(chain);
        }
        if (chain.empty() || chain.back() != base)
            chain.push_back(base);
        const auto key = key_of(f);
        if (!findings_at(chain.back(), codec).contains(key)) {
            b.note = "main does not have the finding, so bisect has nothing "
                     "to find";
            return;
        }
        if (chain.size() < 2 || findings_at(chain.front(), codec).contains(
                key)) {
            b.note = std::format("the finding is already at {}, where bisect "
                "starts; pass an older --bisect-from",
                chain.front().substr(0, 10));
            return;
        }
        const std::vector<std::string> walked(chain.begin() + 1, chain.end());
        const auto at = 1 + first_with(walked, f, codec);
        const auto& merge = chain[at];
        const auto parents = git({"rev-list", "--parents", "-n", "1", merge});
        std::vector<std::string> ids;
        auto rest = parents.empty() ? std::string_view{}
                                    : std::string_view(parents.front());
        while (!rest.empty()) {
            const auto space = rest.find(' ');
            ids.emplace_back(rest.substr(0, space));
            rest = space == std::string_view::npos ? std::string_view{}
                                                   : rest.substr(space + 1);
        }
        std::string culprit = merge;
        if (ids.size() >= 2) {
            const auto& first_parent = ids[1];
            std::vector<std::string> run;
            if (findings_at(first_parent, codec).contains(key))
                run = git({"rev-list", "--first-parent", "--reverse",
                    chain[at - 1] + ".." + first_parent});
            else if (ids.size() == 3 &&
                     findings_at(ids[2], codec).contains(key))
                run = git({"rev-list", "--first-parent", "--reverse",
                    first_parent + ".." + ids[2]});
            if (!run.empty())
                culprit = run[first_with(run, f, codec)];
        }
        b.note = std::format("bisected {} merge{} from {}", chain.size() - 1,
            chain.size() == 2 ? "" : "s", chain.front().substr(0, 10));
        b.culprits.push_back(culprit_of(culprit));
    }
};

}

codec::types::blame_method blame_method_of(const types::options& o,
    std::string_view rule) {
    const auto slash = rule.find('/');
    if (slash == std::string_view::npos)
        return blame_method::bisect;
    const auto codec = rule.substr(0, slash);
    const auto name = rule.substr(slash + 1);
    if (name == codec::ops::drift_rule.name)
        return codec::ops::drift_rule.blame;
    const auto in = [&](std::span<const ct::rule> rules) {
        const auto it = std::ranges::find(rules, name, &ct::rule::name);
        return it == rules.end() ? std::optional<blame_method>()
                                 : it->blame;
    };
    if (codec == codec::ops::exception_codec)
        return in(codec::ops::exception_rules).value_or(blame_method::bisect);
    if (const auto it = std::ranges::find(codecs(), codec,
            &ct::descriptor::name); it != codecs().end())
        return in(it->rules).value_or(blame_method::bisect);
    if (o.checks)
        for (const auto& k : o.checks->checks)
            if (k->codec.name == codec)
                return in(k->codec.rules).value_or(blame_method::bisect);
    return blame_method::bisect;
}

types::blame_report blame(const types::options& o,
    std::span<const types::finding> findings,
    const types::document_ids& documents,
    const ct::dependency_record& record) {
    const auto start = std::chrono::steady_clock::now();
    types::blame_report r;
    if (findings.empty())
        return r;
    tracer t{.o = o, .documents = documents, .record = record, .trailers = {},
        .notify = {}, .base = {}, .read = {}, .probed = {}, .probes = 0};
    const auto rules = ops::provenance_rules_of(setting_of(o,
        ops::provenance_codec, codec::ops::data_option), "false");
    if (rules) {
        t.trailers = rules->trailers;
        t.notify = setting_of(o, ops::provenance_codec, "notify");
        if (t.notify.empty()) {
            const auto it = std::ranges::find_if(t.trailers,
                [](const auto& x) { return x.type.empty(); });
            if (it != t.trailers.end())
                t.notify = it->trailer;
        }
    }
    r.notify = t.notify;
    std::vector<std::string> resolved;
    try {
        resolved.push_back(store::ops::to_hex(
            io::resolve_base(o.checkout, "").id));
    } catch (const std::runtime_error&) {
        for (const std::string_view name : {"main", "HEAD"}) {
            resolved = t.git({"rev-parse", "--verify", "--quiet",
                std::string(name) + "^{commit}"});
            if (!resolved.empty())
                break;
        }
    }
    if (resolved.empty()) {
        // A checkout outside git, or with no commit yet, has nothing to
        // blame; one whose git does not answer must say so.
        std::error_code ignored;
        if (std::filesystem::exists(o.checkout / ".git", ignored) &&
            t.git({"rev-parse", "--git-dir"}).empty())
            r.not_run = "git did not answer in " +
                o.checkout.generic_string();
        return r;
    }
    t.base = resolved.front();

    const bool may_bisect = o.blame == types::blame_mode::bisect;
    for (const auto& f : findings)
        r.findings.push_back({.found = f,
            .method = blame_method_of(o, f.rule),
            .subject = {}, .culprits = {}, .note = {}});

    std::map<std::string, std::vector<std::size_t>> by_path;
    for (std::size_t i = 0; i < r.findings.size(); ++i) {
        auto& b = r.findings[i];
        switch (b.method) {
        case blame_method::line:
            by_path[b.found.path].push_back(i);
            break;
        case blame_method::added: t.blame_added(b); break;
        case blame_method::drift: t.blame_drift(b); break;
        case blame_method::bisect: break;
        }
    }
    for (auto& [path, which] : by_path)
        t.blame_lines(path, std::move(which), r.findings);

    for (auto& b : r.findings) {
        if (!b.culprits.empty())
            continue;
        if (!may_bisect) {
            if (b.method == blame_method::bisect)
                b.note = "bisect is off; check --blame=bisect would run it";
            else
                b.note += "; check --blame=bisect would bisect it";
            continue;
        }
        const auto why = b.method == blame_method::bisect ? std::string()
            : std::string(ops::method_name(b.method)) + " found nothing (" +
                b.note + "); ";
        b.method = blame_method::bisect;
        b.note.clear();
        t.bisect(b);
        b.note = why + b.note;
    }
    r.probes = t.probes;
    r.ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    return r;
}

}
