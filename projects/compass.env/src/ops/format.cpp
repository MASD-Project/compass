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
#include "compass.env/ops/format.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include "compass.env/ops/templates.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::env::types {

bool variable::per_service() const {
    return name.find("{service.") != std::string::npos;
}

std::uint32_t env_format::current() const {
    std::uint32_t out = 0;
    for (const auto& v : versions)
        out = std::max(out, v.number);
    return out;
}

}

namespace compass::env::ops {

namespace {

namespace cops = compass::codec::ops;
using types::finding;
using failure = std::unexpected<finding>;

constexpr std::array<std::string_view, 2> setting_columns{"setting",
    "value"};
constexpr std::array<std::string_view, 6> version_columns{"version", "date",
    "adds", "removes", "format only", "description"};
constexpr std::array<std::string_view, 10> variable_columns{"variable",
    "kind", "argument", "added", "removed", "section", "comment", "when",
    "services", "purpose"};
constexpr std::array<std::string_view, 2> comment_columns{"comment", "text"};
constexpr std::array<std::string_view, 2> input_columns{"input", "purpose"};

constexpr std::array<cops::table_need, 3> needed{{
    {"Settings", setting_columns},
    {"Versions", version_columns},
    {"Variables", variable_columns}}};

constexpr std::array<std::pair<std::string_view, types::value_kind>, 9>
    kinds{{{"value", types::value_kind::value},
        {"offset", types::value_kind::offset},
        {"secret", types::value_kind::secret},
        {"kept", types::value_kind::kept},
        {"inherited", types::value_kind::inherited},
        {"environment", types::value_kind::environment},
        {"version", types::value_kind::version},
        {"file", types::value_kind::file},
        {"carried", types::value_kind::carried}}};

/**
 * @brief The cells of one row of a table, by column name.
 */
struct row_reader {
    const cops::data_table& table;
    std::size_t row;

    [[nodiscard]] std::string operator[](std::string_view column) const {
        const auto c = table.column(column);
        const auto& cells = table.rows[row];
        return c < cells.size() ? cells[c] : std::string{};
    }

    [[nodiscard]] std::uint32_t line() const { return table.row_lines[row]; }
};

std::optional<std::uint32_t> number_of(std::string_view s) {
    std::uint32_t n = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (ec != std::errc{} || end != s.data() + s.size())
        return std::nullopt;
    return n;
}

finding problem(std::uint32_t line, std::string message, std::string fix) {
    return {.rule = unreadable, .line = line, .message = std::move(message),
        .fix = std::move(fix)};
}

bool valid_name(std::string_view name) {
    std::size_t i = 0;
    while (i < name.size()) {
        const char c = name[i];
        if (c == '{') {
            const auto close = name.find('}', i);
            if (close == std::string_view::npos ||
                !name.substr(i + 1, close - i - 1).starts_with("service.") ||
                close == i + 9)
                return false;
            i = close + 1;
            continue;
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '_'))
            return false;
        ++i;
    }
    return !name.empty();
}

/**
 * @brief The templates of a variable that its kind expands.
 */
std::vector<std::string_view> templates_of(const types::variable& v) {
    std::vector<std::string_view> out{v.name, v.when};
    switch (v.kind) {
    case types::value_kind::value:
    case types::value_kind::kept:
    case types::value_kind::inherited:
    case types::value_kind::environment:
    case types::value_kind::file:
    case types::value_kind::offset:
        out.push_back(v.argument);
        break;
    case types::value_kind::secret:
    case types::value_kind::version:
    case types::value_kind::carried:
        break;
    }
    return out;
}

std::expected<void, finding> read_settings(const cops::data_table& t,
    types::env_format& f) {
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        const row_reader row{t, r};
        const auto key = row["setting"];
        auto value = row["value"];
        if (key == "file")
            f.file = std::move(value);
        else if (key == "version_variable")
            f.version_variable = std::move(value);
        else if (key == "service_registry")
            f.service_registry = std::move(value);
        else if (key == "regenerate")
            f.regenerate = std::move(value);
        else if (key == "baseline") {
            const auto n = number_of(value);
            if (!n)
                return failure(problem(row.line(), std::format("the "
                    "baseline \"{}\" is not a version number", value),
                    "Give the version up to which versions need declare "
                    "no variables."));
            f.baseline = *n;
        } else
            return failure(problem(row.line(), std::format("no setting is "
                "named \"{}\"", key), "Name one of file, version_variable, "
                "service_registry, regenerate and baseline."));
    }
    if (f.version_variable.empty())
        return failure(problem(t.line, "the Settings table gives no "
            "version_variable", "Add a row version_variable naming the "
            "variable that carries the version."));
    if (f.file.empty())
        return failure(problem(t.line, "the setting file is empty",
            "Give the file's path, relative to the checkout."));
    return {};
}

std::expected<void, finding> read_versions(const cops::data_table& t,
    types::env_format& f) {
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        const row_reader row{t, r};
        const auto number = number_of(row["version"]);
        if (!number || *number == 0)
            return failure(problem(row.line(), std::format("\"{}\" is not a "
                "version number", row["version"]), "Give a whole number "
                "above 0."));
        if (!f.versions.empty() && *number <= f.versions.back().number)
            return failure(problem(row.line(), std::format("version {} "
                "follows version {}; versions rise down the table", *number,
                f.versions.back().number), "Order the rows, or correct the "
                "number."));
        const auto only = row["format only"];
        if (!only.empty() && only != "yes" && only != "no")
            return failure(problem(row.line(), std::format("format only is "
                "\"{}\"", only), "Write yes, no, or nothing."));
        f.versions.push_back({.number = *number,
            .date = row["date"],
            .adds = list_of(row["adds"]),
            .removes = list_of(row["removes"]),
            .format_only = only == "yes",
            .description = row["description"],
            .line = row.line()});
    }
    return {};
}

std::expected<types::variable, finding> read_variable(const row_reader& row) {
    types::variable v;
    v.name = row["variable"];
    v.argument = row["argument"];
    v.section = row["section"];
    v.comment = row["comment"];
    v.when = row["when"];
    v.purpose = row["purpose"];
    v.line = row.line();
    const auto kind = row["kind"];
    const auto removed = row["removed"];
    const auto k = std::ranges::find(kinds, kind,
        &std::pair<std::string_view, types::value_kind>::first);
    if (k != kinds.end())
        v.kind = k->second;
    else if (!(kind.empty() && !removed.empty()))
        return std::unexpected(problem(row.line(), std::format("{} has the "
            "kind \"{}\"", v.name, kind), "Name one of value, offset, "
            "secret, kept, inherited, environment, version, file and "
            "carried."));
    if (v.kind != types::value_kind::carried && !valid_name(v.name))
        return std::unexpected(problem(row.line(), std::format("\"{}\" is "
            "not a variable name", v.name), "Use letters, digits and '_', "
            "and {service.<field>} for a per-service variable."));
    for (const auto& [column, target] : {std::pair{"added", &v.added},
             std::pair{"removed", &v.removed}}) {
        const auto cell = row[column];
        if (cell.empty())
            continue;
        const auto n = number_of(cell);
        if (!n || *n == 0)
            return std::unexpected(problem(row.line(), std::format("{} "
                "names the version \"{}\" in {}", v.name, cell, column),
                "Give a version number of the Versions table."));
        *target = *n;
    }
    for (auto& word : list_of(row["services"])) {
        const bool negated = word.starts_with('!');
        if (!v.services.empty() && negated != v.excludes_services)
            return std::unexpected(problem(row.line(), std::format("{} both "
                "names and excludes services", v.name), "List the services "
                "it is for, or those it is not for with !, not both."));
        v.excludes_services = negated;
        v.services.push_back(negated ? word.substr(1) : std::move(word));
    }
    if (!v.services.empty() && !v.per_service())
        return std::unexpected(problem(row.line(), std::format("{} names "
            "services but is not a per-service variable", v.name),
            "Put {service.<field>} in its name, or empty services."));
    if (v.kind == types::value_kind::secret && !v.argument.empty()) {
        const auto n = number_of(v.argument);
        if (!n || *n == 0 || *n > 4096)
            return std::unexpected(problem(row.line(), std::format("{} is a "
                "secret of length \"{}\"", v.name, v.argument), "Give a "
                "length from 1 to 4096, or nothing for 32."));
    }
    if (v.kind == types::value_kind::carried) {
        try {
            const platform::text::regex compiled(v.argument);
        } catch (const std::invalid_argument& e) {
            return std::unexpected(problem(row.line(), std::format("{}'s "
                "pattern does not compile: {}", v.name, e.what()),
                "Correct the regular expression."));
        }
    }
    return v;
}

std::expected<void, finding> read_variables(const cops::data_table& t,
    types::env_format& f) {
    for (std::size_t r = 0; r < t.rows.size(); ++r) {
        auto v = read_variable(row_reader{t, r});
        if (!v)
            return failure(v.error());
        f.variables.push_back(std::move(*v));
    }
    return {};
}

/**
 * @brief Whether every name the live rows, the comments and the settings
 * use is known when it is used, and the blocks they name exist.
 */
std::expected<void, finding> check_references(const types::env_format& f) {
    std::set<std::string, std::less<>> inputs(builtin_inputs.begin(),
        builtin_inputs.end());
    for (const auto& i : f.inputs)
        inputs.insert(i.name);
    std::set<std::string, std::less<>> all_variables;
    for (const auto& v : f.variables)
        if (v.removed == 0 && !v.per_service() &&
            v.kind != types::value_kind::carried)
            all_variables.insert(v.name);
    for (const auto& i : f.inputs)
        if (all_variables.contains(i.name))
            return failure(problem(0, std::format("{} is both an input and "
                "a variable", i.name), "Rename the input."));

    std::set<std::string, std::less<>> earlier;
    std::set<std::string, std::less<>> seen;
    std::size_t versions = 0;
    for (const auto& v : f.variables) {
        if (v.removed != 0)
            continue;
        if (!v.per_service() && v.kind != types::value_kind::carried &&
            v.services.empty() && !seen.insert(v.name).second)
            return failure(problem(v.line, std::format("{} has two rows",
                v.name), "Remove one, or mark it removed."));
        if (v.kind == types::value_kind::version) {
            ++versions;
            if (v.name != f.version_variable)
                return failure(problem(v.line, std::format("{} is of kind "
                    "version, and the version_variable is {}", v.name,
                    f.version_variable), "Make the version_variable's row "
                    "the one of kind version."));
        }
        for (const auto& block : {v.section, v.comment})
            if (!block.empty() && !f.comments.contains(block))
                return failure(problem(v.line, std::format("{} names the "
                    "comment \"{}\", which the Comments table lacks", v.name,
                    block), "Add the comment's lines to the Comments "
                    "table."));
        for (const auto text : templates_of(v)) {
            const auto names = names_in(text);
            if (!names)
                return failure(problem(v.line, std::format("{}: {}", v.name,
                    names.error()), "Correct the template."));
            for (const auto& n : *names) {
                const bool service_field = n.starts_with("service.");
                if (service_field && v.per_service())
                    continue;
                if (inputs.contains(n) || earlier.contains(n) ||
                    (text == v.when && n == v.name))
                    continue;
                return failure(problem(v.line, std::format("{} uses {}, "
                    "which is {}", v.name, n, service_field
                        ? "a service field, and the variable is not "
                          "per-service"
                        : all_variables.contains(n)
                        ? "a variable of a later row"
                        : "no input or variable"), "Declare the input in "
                    "the Inputs table, or use a variable of an earlier "
                    "row."));
            }
        }
        if (!v.per_service() && v.kind != types::value_kind::carried)
            earlier.insert(v.name);
    }
    if (versions != 1)
        return failure(problem(0, std::format("the Variables table has {} "
            "rows of kind version", versions), "Give the version_variable "
            "one row, of kind version."));
    for (const auto& [block, lines] : f.comments)
        for (const auto& text : lines) {
            const auto names = names_in(text);
            if (!names)
                return failure(problem(0, std::format("the comment {}: {}",
                    block, names.error()), "Correct the line."));
            for (const auto& n : *names)
                if (!inputs.contains(n) && !all_variables.contains(n))
                    return failure(problem(0, std::format("the comment {} "
                        "uses {}, which is no input or variable", block, n),
                        "Declare the input in the Inputs table."));
        }
    return {};
}

}

std::vector<std::string> list_of(std::string_view cell) {
    std::vector<std::string> out;
    std::size_t at = 0;
    while (at < cell.size()) {
        const auto begin = cell.find_first_not_of(", \t", at);
        if (begin == std::string_view::npos)
            break;
        const auto end = std::min(cell.find_first_of(", \t", begin),
            cell.size());
        out.emplace_back(cell.substr(begin, end - begin));
        at = end;
    }
    return out;
}

std::expected<types::env_format, finding> read_format(
    const cops::data_tables& tables) {
    const auto checked = cops::tables_for(codec_name,
        cops::encode_data_tables(tables), needed);
    if (!checked)
        return failure(problem(0, checked.error(), "Give the tables the "
            "message names."));
    types::env_format f;
    f.source = tables.source;
    if (const auto r = read_settings(*tables.find("Settings"), f); !r)
        return failure(r.error());
    if (const auto r = read_versions(*tables.find("Versions"), f); !r)
        return failure(r.error());
    if (const auto* t = tables.find("Comments")) {
        for (const auto c : comment_columns)
            if (t->column(c) == t->columns.size())
                return failure(problem(t->line, std::format("the Comments "
                    "table has no column \"{}\"", c), "Add the column."));
        for (std::size_t r = 0; r < t->rows.size(); ++r) {
            const row_reader row{*t, r};
            f.comments[row["comment"]].push_back(row["text"]);
        }
    }
    if (const auto* t = tables.find("Inputs")) {
        for (const auto c : input_columns)
            if (t->column(c) == t->columns.size())
                return failure(problem(t->line, std::format("the Inputs "
                    "table has no column \"{}\"", c), "Add the column."));
        for (std::size_t r = 0; r < t->rows.size(); ++r) {
            const row_reader row{*t, r};
            if (std::ranges::contains(builtin_inputs, row["input"]))
                return failure(problem(row.line(), std::format("{} is an "
                    "input compass gives", row["input"]), "Take the row "
                    "out."));
            f.inputs.push_back({row["input"], row["purpose"]});
        }
    }
    if (const auto r = read_variables(*tables.find("Variables"), f); !r)
        return failure(r.error());
    const bool expands = std::ranges::any_of(f.variables,
        [](const auto& v) { return v.removed == 0 && v.per_service(); });
    if (expands && f.service_registry.empty())
        return failure(problem(0, "a per-service variable needs the "
            "setting service_registry", "Name the service registry's path "
            "in the Settings table."));
    if (const auto r = check_references(f); !r)
        return failure(r.error());
    return f;
}

std::vector<finding> check_format(const types::env_format& f) {
    std::vector<finding> out;
    const auto version_of = [&](std::uint32_t n) -> const types::version* {
        const auto it = std::ranges::find(f.versions, n,
            &types::version::number);
        return it == f.versions.end() ? nullptr : &*it;
    };
    for (const auto& v : f.variables) {
        if (v.added == 0) {
            out.push_back({.rule = unknown_version, .line = v.line,
                .message = std::format("{} names no version that added it",
                    v.name),
                .fix = std::format("Record the version that adds it: "
                    "compass env version new \"<description>\" --adds {}",
                    v.name)});
            continue;
        }
        for (const auto& [n, adding] : {std::pair{v.added, true},
                 std::pair{v.removed, false}}) {
            if (n == 0)
                continue;
            const auto* version = version_of(n);
            if (version == nullptr) {
                out.push_back({.rule = unknown_version, .line = v.line,
                    .message = std::format("{} is {} in version {}, which the "
                        "Versions table lacks", v.name,
                        adding ? "added" : "removed", n),
                    .fix = std::format("Record the version: compass env "
                        "version new \"<description>\" --{} {}",
                        adding ? "adds" : "removes", v.name)});
                continue;
            }
            const auto& names = adding ? version->adds : version->removes;
            if (n > f.baseline && !std::ranges::contains(names, v.name))
                out.push_back({.rule = undeclared, .line = v.line,
                    .message = std::format("{} says version {} {} it, and "
                        "version {} does not say so; a variable added "
                        "without a new version leaves every environment's "
                        "file current and without it", v.name, n,
                        adding ? "added" : "removed", n),
                    .fix = std::format("Record a new version: compass env "
                        "version new \"<description>\" --{} {}",
                        adding ? "adds" : "removes", v.name)});
        }
    }
    for (const auto& version : f.versions) {
        for (const auto& [names, adding] : {std::pair{&version.adds, true},
                 std::pair{&version.removes, false}}) {
            for (const auto& name : *names) {
                const bool found = std::ranges::any_of(f.variables,
                    [&](const auto& v) {
                        return v.name == name &&
                            (adding ? v.added : v.removed) == version.number;
                    });
                if (!found)
                    out.push_back({.rule = unknown_variable,
                        .line = version.line,
                        .message = std::format("version {} {} {}, and no "
                            "row of the Variables table is {} in it",
                            version.number, adding ? "adds" : "removes",
                            name, adding ? "added" : "removed"),
                        .fix = std::format("Add the row of {}, or correct "
                            "its {} version.", name,
                            adding ? "added" : "removed")});
            }
        }
        if (version.number > f.baseline && version.adds.empty() &&
            version.removes.empty() && !version.format_only)
            out.push_back({.rule = empty_version, .line = version.line,
                .message = std::format("version {} adds and removes no "
                    "variable and is not format only", version.number),
                .fix = "Name the variables it adds or removes, or mark it "
                       "format only when it changes only values, layout or "
                       "the services the registry expands."});
    }
    return out;
}

}
