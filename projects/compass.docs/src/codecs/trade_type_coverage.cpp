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
#include "compass.docs/codecs/trade_type_coverage.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::string_view mined_path =
    "external/ore/catalogue/trade_types.tsv";
constexpr std::string_view version_path =
    "external/ore/catalogue/ore_version.txt";
constexpr std::string_view catalogue_path =
    "projects/ores.trading/modeling/ores.trading.trade_type_catalogue.org";
constexpr std::string_view recorded_dir =
    "projects/ores.synthetic/core/tests/data/generated_trades/";
constexpr std::string_view story_dir = "doc/agile/versions/v0/sprint_30/"
    "hold_every_product_type_the_engine_supports/";
constexpr std::string_view document_path =
    "doc/knowledge/domain/ore_trade_type_coverage.org";
constexpr std::string_view tag_task = "80315472-7360-4C53-B6F7-57F7BCE78A1C";
constexpr std::string_view form_task = "8F1B7D90-0D95-405B-845C-207AA1C92BE3";
constexpr std::string_view both_forms_task =
    "589884B5-1B2C-4044-AAD4-40680C9F3F0C";
constexpr std::string_view begin_marker = "# BEGIN GENERATED";
constexpr std::string_view end_marker = "# END GENERATED";

/**
 * @brief A source the script rejects, or would fail on, which stops it.
 */
struct stopped : std::runtime_error {
    using std::runtime_error::runtime_error;
};

using row = std::map<std::string, std::string, std::less<>>;

const std::string& at(const row& r, std::string_view key) {
    const auto it = r.find(key);
    if (it == r.end())
        throw stopped("no column " + std::string(key));
    return it->second;
}

std::vector<std::string_view> split(std::string_view text, char by) {
    std::vector<std::string_view> out;
    for (std::size_t i = 0;;) {
        const auto j = text.find(by, i);
        out.push_back(text.substr(i, j == std::string_view::npos
            ? std::string_view::npos : j - i));
        if (j == std::string_view::npos)
            return out;
        i = j + 1;
    }
}

/**
 * @brief str.split() with no argument: the runs of non-blank characters.
 */
std::vector<std::string_view> words(std::string_view text) {
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (true) {
        i = co::skip_spaces(text, i);
        if (i >= text.size())
            return out;
        auto j = i;
        while (j < text.size() && !co::is_space(text[j]))
            ++j;
        out.push_back(text.substr(i, j - i));
        i = j;
    }
}

/**
 * @brief The cells of a table line: line.strip().strip("|").split("|"),
 * each stripped.
 */
std::vector<std::string> cells_of(std::string_view line) {
    auto inner = co::strip(line);
    while (inner.starts_with('|'))
        inner.remove_prefix(1);
    while (inner.ends_with('|'))
        inner.remove_suffix(1);
    std::vector<std::string> out;
    for (const auto c : split(inner, '|'))
        out.emplace_back(co::strip(c));
    return out;
}

std::vector<row> mined_rows(std::string_view text) {
    const auto lines = co::split_lines(text);
    if (lines.empty())
        throw stopped("no header");
    const auto header = split(lines[0], '\t');
    std::vector<row> out;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        const auto values = split(lines[i], '\t');
        row r;
        for (std::size_t k = 0; k < std::min(header.size(), values.size());
             ++k)
            r[std::string(header[k])] = values[k];
        out.push_back(std::move(r));
    }
    return out;
}

/**
 * @brief The rows of the table under the heading @p heading of the
 * catalogue: the lines that start with "| " while it is the last heading.
 */
std::vector<std::string_view> section_rows(std::string_view text,
    std::string_view heading) {
    std::vector<std::string_view> out;
    bool inside = false;
    for (const auto line : co::split_lines(text)) {
        if (line.starts_with("* ")) {
            inside = co::strip(line) == heading;
            continue;
        }
        if (inside && line.starts_with("| "))
            out.push_back(line);
    }
    return out;
}

struct entry {
    std::string construction;
    std::string instrument;
};

std::map<std::string, entry, std::less<>> catalogue(std::string_view text) {
    std::map<std::string, entry, std::less<>> rows;
    std::optional<std::vector<std::string>> header;
    for (const auto line : section_rows(text, "* Trade types")) {
        auto cells = cells_of(line);
        if (!header) {
            for (const auto need : {"code", "construction", "instrument"})
                if (std::ranges::find(cells, need) == cells.end())
                    throw stopped("the trade types table has no column");
            header = std::move(cells);
            continue;
        }
        row r;
        for (std::size_t k = 0; k < std::min(header->size(), cells.size());
             ++k)
            r[(*header)[k]] = cells[k];
        rows[at(r, "code")] = {at(r, "construction"), at(r, "instrument")};
    }
    return rows;
}

const entry& entry_of(const std::map<std::string, entry, std::less<>>& cat,
    std::string_view code) {
    const auto it = cat.find(code);
    if (it == cat.end())
        throw stopped("the catalogue has no " + std::string(code));
    return it->second;
}

std::set<std::string, std::less<>> unsupported(std::string_view text) {
    std::set<std::string, std::less<>> codes;
    for (const auto line : section_rows(text, "* Unsupported trade types"))
        if (!line.starts_with("| code"))
            codes.insert(cells_of(line).front());
    return codes;
}

struct task {
    std::string id;
    std::string title;
    std::string description;
};

std::string field(std::string_view text, std::string_view pattern) {
    const pt::regex re(pattern, {.multiline = true});
    const auto m = re.search(text);
    if (!m)
        throw stopped("no line matches " + std::string(pattern));
    return std::string(m->group(1));
}

std::vector<task> story_tasks(const ct::corpus_view& v) {
    auto paths = v.list_files(std::string(story_dir) + "task_", ".org");
    std::erase_if(paths, [](std::string_view p) {
        return p.find('/', story_dir.size()) != std::string_view::npos;
    });
    std::vector<task> tasks;
    std::set<std::string, std::less<>> ids;
    for (const auto path : paths) {
        const auto text = v.read_file(path).value_or("");
        const auto id = field(text, R"(:ID: (\S+))");
        ids.insert(id);
        if (field(text, R"(\| State\s+\| (\w+))") == "ABANDONED")
            continue;
        tasks.push_back({id, field(text, R"(^#\+title: Task: (.*))"),
            field(text, R"(^#\+description: (.*))")});
    }
    for (const auto id : {tag_task, form_task, both_forms_task})
        if (!ids.contains(id))
            throw stopped("a task constant is not in the story");
    return tasks;
}

bool disagrees(const row& r, const entry& e) {
    if (at(r, "registered") != "yes")
        return false;
    const auto& form = at(r, "form");
    if (form == "native")
        return e.construction != "native" && e.construction != "container";
    if (form == "scripted")
        return e.construction != "scripted" && e.construction != "script";
    throw stopped("a form that is not native or scripted");
}

std::string code(std::string_view value) {
    return value.empty() || value == "-" ? "-" : "=" + std::string(value) + "=";
}

std::string joined(const std::vector<std::string>& parts,
    std::string_view by) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i)
        (out += i > 0 ? by : "") += parts[i];
    return out;
}

std::string closing(const row& r, const entry& e,
    const std::vector<task>& tasks) {
    std::set<std::string_view> kinds;
    if (at(r, "registered") == "no")
        kinds.insert(tag_task);
    if (disagrees(r, e))
        kinds.insert(form_task);
    if (at(r, "library_scripts") != "-")
        kinds.insert(both_forms_task);
    const pt::regex named_type("\\b" + pt::escape(at(r, "trade_type")) +
        "\\b");
    std::vector<std::string> named;
    for (const auto& t : tasks)
        if (kinds.contains(t.id) || named_type.search(t.description))
            named.push_back("[[id:" + t.id + "][" + t.title + "]]");
    return named.empty() ? "-" : joined(named, ", ");
}

class generated_trades {
public:
    explicit generated_trades(const ct::corpus_view& v) : v_(&v) {}

    [[nodiscard]] bool recorded(std::string_view code) const {
        return v_->file_exists(std::string(recorded_dir) + std::string(code) +
            ".xml");
    }

private:
    const ct::corpus_view* v_;
};

std::vector<std::string> table(const std::vector<row>& rows,
    const std::map<std::string, entry, std::less<>>& cat,
    const std::vector<task>& tasks, const generated_trades& trades) {
    std::vector<std::string> out{"| Type | ORE form | Catalogue form | Data "
        "element | Table | Generated | Library scripts | Match | Closed by |",
        "|-"};
    for (const auto& r : rows) {
        if (at(r, "in_catalogue") == "no")
            continue;
        const auto& type = at(r, "trade_type");
        const auto& e = entry_of(cat, type);
        const auto element = code(at(r, "element")) +
            (at(r, "element_in_schema") == "yes" ? "" : " (not in the XSD)");
        std::string scripts = "-";
        if (at(r, "library_scripts") != "-") {
            std::vector<std::string> each;
            for (const auto s : words(at(r, "library_scripts")))
                each.push_back(code(s));
            scripts = joined(each, ", ");
        }
        const auto form = at(r, "registered") == "no" ? "not registered"
            : at(r, "form");
        out.push_back("| " + joined({code(type), form, e.construction,
            element, code(e.instrument), trades.recorded(type) ? "yes" : "no",
            scripts, at(r, "script_match"), closing(r, e, tasks)}, " | ") +
            " |");
    }
    return out;
}

std::vector<std::string> counts(const std::vector<row>& rows,
    const std::map<std::string, entry, std::less<>>& cat,
    const generated_trades& trades) {
    std::size_t declared = 0, registered = 0, scripted = 0, routed = 0,
        generated = 0, disagree = 0, twins = 0;
    std::vector<std::string> unregistered;
    std::vector<std::string> only_ore;
    for (const auto& r : rows) {
        const auto& type = at(r, "trade_type");
        if (at(r, "in_catalogue") != "yes") {
            if (at(r, "in_catalogue") == "no")
                only_ore.push_back(code(type));
            continue;
        }
        ++declared;
        const auto& e = entry_of(cat, type);
        if (at(r, "registered") == "yes") {
            ++registered;
            if (at(r, "form") == "scripted")
                ++scripted;
            if (disagrees(r, e))
                ++disagree;
        } else if (at(r, "registered") == "no") {
            unregistered.push_back(code(type));
        }
        if (!e.instrument.empty())
            ++routed;
        if (trades.recorded(type))
            ++generated;
        if (at(r, "library_scripts") != "-")
            ++twins;
    }
    const auto n = [](std::size_t x) { return std::to_string(x); };
    return {
        "- The catalogue declares " + n(declared) + " types. ORE registers " +
            n(registered) + " of them, " + n(scripted) + " read through a "
            "=ScriptedTrade= class and " + n(registered - scripted) +
            " natively.",
        "- " + n(declared - registered) + " catalogue types have no ORE trade "
            "builder: " + joined(unregistered, ", ") + ".",
        "- ORE registers " + n(only_ore.size()) + " type the catalogue leaves "
            "out: " + joined(only_ore, ", ") + ".",
        "- " + n(routed) + " types name an instrument table, and the "
            "generator records " + n(generated) + ".",
        "- " + n(disagree) + " registered types state a form in the "
            "catalogue that ORE does not read them as.",
        "- " + n(twins) + " types have a library script that carries the "
            "same product."};
}

void coverage(const ct::corpus_view& v, ct::file_output& out) {
    try {
        const auto rows = mined_rows(v.read_file(mined_path).value_or(""));
        const auto catalogue_text = v.read_file(catalogue_path).value_or("");
        const auto cat = catalogue(catalogue_text);
        const auto tasks = story_tasks(v);
        const auto version_text = v.read_file(version_path).value_or("");
        const auto version = words(version_text);
        if (version.size() < 3)
            throw stopped("no commit in the version file");
        const generated_trades trades(v);
        std::vector<std::string> block{std::string(begin_marker), "",
            "Read at ORE commit =" + std::string(version[2]) + "=. Regenerate "
            "with =build/scripts/generate_trade_type_coverage.py --write=.",
            "", "** Counts", ""};
        for (auto& line : counts(rows, cat, trades))
            block.push_back(std::move(line));
        block.insert(block.end(), {"", "** Trade types", ""});
        for (auto& line : table(rows, cat, tasks, trades))
            block.push_back(std::move(line));
        block.insert(block.end(), {"", std::string(end_marker)});

        const auto text = v.read_file(document_path);
        if (!text || co::count(*text, begin_marker) != 1 ||
            co::count(*text, end_marker) != 1)
            return;
        for (const auto& r : rows)
            if (at(r, "in_catalogue") == "yes" &&
                disagrees(r, entry_of(cat, at(r, "trade_type"))))
                return;
        const auto tagged = unsupported(catalogue_text);
        std::set<std::string, std::less<>> unregistered;
        for (const auto& r : rows)
            if (at(r, "in_catalogue") == "yes" && at(r, "registered") == "no")
                unregistered.insert(at(r, "trade_type"));
        if (tagged != unregistered || std::ranges::any_of(tagged,
                [&](const std::string& c) { return trades.recorded(c); }))
            return;
        const auto head = text->substr(0, text->find(begin_marker));
        const auto tail = text->substr(text->find(end_marker) +
            end_marker.size());
        out.files.add(document_path, head + joined(block, "\n") + tail);
    } catch (const stopped&) {
        return;
    }
}

}

const ct::descriptor trade_type_coverage{
    .name = "trade_type_coverage",
    .version = 1,
    .summary = "Writes the generated block of the ORE trade type coverage "
        "note.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = {},
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::file_output>{coverage}};

}
