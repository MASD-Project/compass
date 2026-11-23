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
#include "compass.db/ops/campaign_page.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <optional>
#include <string_view>
#include "compass.store/ops/blob_hash.hpp"

namespace compass::db::ops {

namespace {

/**
 * @brief @p s on one line: each line break or tab a space, trimmed.
 */
std::string one_line(std::string_view s) {
    std::string out;
    for (const auto ch : s)
        out += ch == '\n' || ch == '\r' || ch == '\t' ? ' ' : ch;
    const auto first = out.find_first_not_of(' ');
    if (first == std::string::npos)
        return {};
    return out.substr(first, out.find_last_not_of(' ') - first + 1);
}

/**
 * @brief @p s as a table cell: on one line, with each | written as the
 * entity \vert{}, which org shows as a bar without ending the cell.
 */
std::string cell(std::string_view s) {
    std::string out;
    for (const auto ch : one_line(s)) {
        if (ch == '|')
            out += "\\vert{}";
        else
            out += ch;
    }
    return out;
}

/**
 * @brief The width org gives @p s: its code points, not its bytes.
 */
std::size_t width(std::string_view s) {
    return static_cast<std::size_t>(std::ranges::count_if(s,
        [](char ch) { return (static_cast<unsigned char>(ch) & 0xC0) != 0x80; }));
}

/**
 * @brief An org table, its first row the header, aligned as org aligns
 * one. The cells are written as given, so each must already be a cell.
 */
class table {
public:
    explicit table(std::vector<std::string> header) {
        rows_.push_back(std::move(header));
    }

    void add(std::vector<std::string> row) { rows_.push_back(std::move(row)); }

    void write(std::string& out) const {
        std::vector<std::size_t> widths(rows_.front().size(), 0);
        for (const auto& row : rows_)
            for (std::size_t i = 0; i < row.size(); ++i)
                widths[i] = std::max(widths[i], width(row[i]));
        const auto line = [&](const std::vector<std::string>& row) {
            out += "|";
            for (std::size_t i = 0; i < row.size(); ++i)
                out += " " + row[i] +
                    std::string(widths[i] - width(row[i]), ' ') + " |";
            out += "\n";
        };
        line(rows_.front());
        out += "|";
        for (std::size_t i = 0; i < widths.size(); ++i)
            out += std::string(widths[i] + 2, '-') +
                (i + 1 < widths.size() ? "+" : "|");
        out += "\n";
        for (std::size_t r = 1; r < rows_.size(); ++r)
            line(rows_[r]);
    }

private:
    std::vector<std::vector<std::string>> rows_;
};

/**
 * @brief @p text in an example block, verbatim. A line that org would
 * read as a heading or a keyword, such as the block's own end line, is
 * escaped as org escapes it, with a leading comma, which org drops when it
 * shows the block.
 */
void example(std::string& out, std::string_view text) {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.remove_suffix(1);
    out += "#+begin_example\n";
    std::size_t at = 0;
    while (at <= text.size()) {
        const auto end = std::min(text.find('\n', at), text.size());
        auto line = text.substr(at, end - at);
        if (line.ends_with('\r'))
            line.remove_suffix(1);
        auto rest = line.substr(std::min(line.find_first_not_of(" \t"),
            line.size()));
        rest.remove_prefix(std::min(rest.find_first_not_of(','),
            rest.size()));
        if (rest.starts_with('*') || rest.starts_with("#+"))
            out += ',';
        out += line;
        out += '\n';
        at = end + 1;
    }
    out += "#+end_example\n";
}

std::string link(std::string_view id, std::string_view text) {
    return std::format("[[id:{}][{}]]", id, one_line(text));
}

std::string_view extension_of(std::string_view media_type) {
    if (media_type == "image/png")
        return "png";
    if (media_type == "image/jpeg")
        return "jpg";
    if (media_type == "image/gif")
        return "gif";
    if (media_type == "image/webp")
        return "webp";
    return "bin";
}

std::string_view date_of(std::string_view time) {
    return time.substr(0, std::min<std::size_t>(time.size(), 10));
}

std::string_view folder_of(std::string_view path) {
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string_view()
                                           : path.substr(0, slash);
}

/**
 * @brief The tags a page in the story's folder carries, as a task's do:
 * the story's folder, then its sprint and version when the folder sits at
 * versions/<version>/<sprint>/<story>.
 */
std::string filetags(std::string_view folder) {
    std::vector<std::string_view> parts;
    std::size_t at = 0;
    while (at <= folder.size()) {
        const auto end = std::min(folder.find('/', at), folder.size());
        if (end > at)
            parts.push_back(folder.substr(at, end - at));
        at = end + 1;
    }
    if (parts.empty())
        return "";
    const auto n = parts.size();
    if (n >= 4 && parts[n - 4] == "versions")
        return std::format(":{}:{}:{}:", parts[n - 1], parts[n - 2],
            parts[n - 3]);
    return std::format(":{}:", parts[n - 1]);
}

std::string step_name(std::int32_t ord) {
    return std::format("step {}", ord + 1);
}

/**
 * @brief The name the campaign's :ID: comes from, which every record of
 * the campaign extends with its own.
 */
std::string campaign_name(const types::campaign_rows& c) {
    return std::format("compass test_campaign {} {} {}", c.environment,
        c.story_id, c.id);
}

struct page_context {
    const types::campaign_rows& campaign;
    std::string images_folder;
    std::map<std::string, std::string, std::less<>> titles;
    std::map<std::string, std::string_view, std::less<>> extensions;
};

void screenshots(std::string& out, const page_context& p,
    const std::vector<types::exported_screenshot>& shots) {
    if (shots.empty())
        return;
    out += "\nScreenshots:\n\n";
    for (const auto& s : shots) {
        out += std::format("- [[file:{}/{}.{}]]", p.images_folder,
            s.attachment, p.extensions.find(s.attachment)->second);
        if (s.step_ord)
            out += std::format(", {}", step_name(*s.step_ord));
        if (s.caption && !one_line(*s.caption).empty())
            out += std::format(": {}", one_line(*s.caption));
        out += "\n";
    }
}

void drawer(std::string& out, std::string_view id) {
    out += std::format(":PROPERTIES:\n:ID: {}\n:END:\n", id);
}

void write_run(std::string& out, const page_context& p,
    const types::exported_run& r) {
    const auto outcome = r.finished ? r.finished->outcome
                                    : std::string("in progress");
    out += std::format("\n*** Run {}: {}\n", r.id, outcome);
    drawer(out, derived_id(std::format("{} run {}",
        campaign_name(p.campaign), r.id)));
    out += "\n";
    table fields({"Field", "Value"});
    fields.add({"Tester", cell(r.tester)});
    fields.add({"Started", r.started_at});
    fields.add({"Finished", r.finished ? r.finished->at : "not yet"});
    fields.add({"Definition", r.definition.substr(0, 12)});
    fields.add({"Outcome", outcome});
    fields.write(out);
    out += "\n";
    if (r.steps.empty()) {
        out += "The definition this run followed has no steps.\n";
    } else {
        table steps({"Step", "Title", "Outcome", "Notes"});
        for (const auto& s : r.steps)
            steps.add({std::to_string(s.ord + 1), cell(s.title),
                s.outcome ? cell(*s.outcome) : "-",
                s.notes ? cell(*s.notes) : ""});
        steps.write(out);
    }
    screenshots(out, p, r.screenshots);
}

void write_note(std::string& out, const page_context& p,
    const types::exported_note& n) {
    std::string about = "the campaign";
    if (n.scenario_id) {
        const auto title = p.titles.find(*n.scenario_id);
        about = link(*n.scenario_id, title == p.titles.end()
            ? *n.scenario_id : title->second);
        if (n.step_ord)
            about += ", " + step_name(*n.step_ord);
    }
    out += std::format("\n*** {} Note {} on {}\n",
        n.addressed ? "DONE" : "TODO", n.id, about);
    drawer(out, derived_id(std::format("{} note {}",
        campaign_name(p.campaign), n.id)));
    out += "\n";
    table fields({"Field", "Value"});
    fields.add({"Author", cell(n.author)});
    fields.add({"Written", n.created_at});
    if (n.addressed) {
        fields.add({"Addressed by", cell(n.addressed->by)});
        fields.add({"Addressed", n.addressed->at});
    }
    fields.write(out);
    out += "\n";
    example(out, n.text);
    if (n.addressed && n.addressed->reply) {
        out += "\nThe reply:\n\n";
        example(out, *n.addressed->reply);
    }
    screenshots(out, p, n.screenshots);
}

std::string latest_date(const types::campaign_rows& c) {
    std::string latest = c.opened_at;
    const auto see = [&](std::string_view t) {
        if (t > latest)
            latest = t;
    };
    for (const auto& s : c.scenarios)
        for (const auto& r : s.runs) {
            see(r.started_at);
            if (r.finished)
                see(r.finished->at);
        }
    for (const auto& n : c.notes) {
        see(n.created_at);
        if (n.addressed)
            see(n.addressed->at);
    }
    return std::string(date_of(latest));
}

std::string page(const page_context& p) {
    const auto& c = p.campaign;
    std::string_view story_title = c.story_title;
    if (story_title.starts_with("Story: "))
        story_title.remove_prefix(7);
    const auto environment = one_line(c.environment);
    std::size_t runs = 0;
    std::size_t running = 0;
    for (const auto& s : c.scenarios)
        for (const auto& r : s.runs) {
            ++runs;
            running += r.finished ? 0 : 1;
        }
    const auto open = static_cast<std::size_t>(std::ranges::count_if(
        c.notes, [](const auto& n) { return !n.addressed; }));

    std::string out;
    drawer(out, derived_id(campaign_name(c)));
    out += std::format("#+title: Test Campaign: {} on {}\n",
        one_line(story_title), environment);
    out += std::format("#+description: The test runs and notes of a story's "
        "scenarios on the {} environment.\n", environment);
    out += "#+type: test_campaign\n#+level: s1\n";
    out += std::format("#+filetags: {}\n", filetags(folder_of(c.story_path)));
    out += std::format("#+created: {}\n", date_of(c.opened_at));
    out += std::format("#+updated: {}\n", latest_date(c));
    out += std::format("#+environment: {}\n", environment);
    out += "#+todo: TODO | DONE\n\n";
    out += std::format("This page records a [[id:{}][test campaign]] of {} on "
        "the {} environment. =compass test export= writes it from compass's "
        "database, so an edit here is lost at the next export.\n",
        test_campaign_contract_id, link(c.story_id, c.story_title),
        environment);

    out += "\n* Campaign\n\n";
    table fields({"Field", "Value"});
    fields.add({"Story", link(c.story_id, c.story_title)});
    fields.add({"Sprint", c.sprint ? link(c.sprint->org_id, c.sprint->title)
                                   : "none"});
    fields.add({"Environment", cell(environment)});
    fields.add({"Opened", c.opened_at});
    fields.add({"Runs", std::format("{}, {} in progress", runs, running)});
    fields.add({"Notes", std::format("{} open, {} addressed", open,
        c.notes.size() - open)});
    fields.write(out);

    out += "\n* Scenarios\n";
    for (const auto& s : c.scenarios) {
        out += std::format("\n** {}\n", link(s.org_id, s.title));
        if (s.runs.empty())
            out += "\nNo runs.\n";
        for (const auto& r : s.runs)
            write_run(out, p, r);
    }
    if (c.scenarios.empty())
        out += "\nNo runs.\n";

    out += "\n* Notes\n";
    std::map<std::string, std::vector<const types::exported_note*>> by_kind;
    for (const auto& n : c.notes)
        by_kind[n.kind].push_back(&n);
    for (const auto& [kind, notes] : by_kind) {
        out += std::format("\n** {}\n", one_line(kind));
        for (const auto* n : notes)
            write_note(out, p, *n);
    }
    if (c.notes.empty())
        out += "\nNo notes.\n";
    return out;
}

}

std::string derived_id(std::string_view name) {
    auto digest = store::ops::sha1_of(name);
    digest[6] = static_cast<std::uint8_t>((digest[6] & 0x0F) | 0x50);
    digest[8] = static_cast<std::uint8_t>((digest[8] & 0x3F) | 0x80);
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string out;
    for (std::size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            out += '-';
        out += digits[digest[i] >> 4];
        out += digits[digest[i] & 0x0F];
    }
    return out;
}

std::vector<types::exported_file> render_campaign(
    const types::campaign_rows& c) {
    const auto name = std::format("test_campaign_{}", c.id);
    const auto folder = std::string(folder_of(c.story_path));
    const auto prefix = folder.empty() ? std::string() : folder + "/";
    page_context p{c, name, {}, {}};
    for (const auto& s : c.scenarios)
        p.titles.emplace(s.org_id, s.title);
    for (const auto& i : c.images)
        p.extensions.emplace(i.attachment, extension_of(i.media_type));

    std::vector<types::exported_file> out;
    types::exported_file f;
    f.path = prefix + name + ".org";
    f.bytes = page(p);
    out.push_back(std::move(f));
    for (const auto& i : c.images) {
        types::exported_file image;
        image.path = std::format("{}{}/{}.{}", prefix, name, i.attachment,
            extension_of(i.media_type));
        image.bytes = i.bytes;
        out.push_back(std::move(image));
    }
    return out;
}

}
