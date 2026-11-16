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
#include "compass.codec/ops/tangle.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "compass.org/ops/document_fields.hpp"

namespace compass::codec::ops {

namespace {

namespace ot = compass::org::types;

constexpr std::size_t tab_width = 8;

/**
 * @brief A header argument that leaves the tangled bytes alone: with any
 * value when value is empty, else with that value only.
 */
struct accepted_argument {
    std::string_view name;
    std::string_view value;
};

constexpr std::array<accepted_argument, 10> accepted_arguments{{
    {"results", {}}, {"exports", {}}, {"eval", {}}, {"cache", {}},
    {"session", {}}, {"mkdirp", {}}, {"noweb-ref", {}}, {"padline", "yes"},
    {"comments", "no"}, {"noweb", "no"}}};

bool is_blank(char c) noexcept { return c == ' ' || c == '\t'; }

/**
 * @brief Whitespace as the syntax table of a fundamental-mode buffer has
 * it, which is what \S- tests in org-do-remove-indentation.
 */
bool is_space(char c) noexcept {
    return is_blank(c) || c == '\n' || c == '\r' || c == '\f';
}

/**
 * @brief What org-trim removes.
 */
bool is_trimmed(char c) noexcept {
    return is_blank(c) || c == '\n' || c == '\r';
}

std::vector<std::string_view> split(std::string_view s, char separator) {
    std::vector<std::string_view> parts;
    for (std::size_t first = 0;;) {
        const auto end = s.find(separator, first);
        if (end == std::string_view::npos) {
            parts.push_back(s.substr(first));
            return parts;
        }
        parts.push_back(s.substr(first, end - first));
        first = end + 1;
    }
}

/**
 * @brief org-unescape-code-in-string: drops one comma from each line that
 * is blanks, one or more commas, then * or #+.
 */
std::string unescape(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    const auto lines = split(raw, '\n');
    for (std::size_t k = 0; k < lines.size(); ++k) {
        if (k > 0)
            out.push_back('\n');
        const auto line = lines[k];
        std::size_t blanks = 0;
        while (blanks < line.size() && is_blank(line[blanks]))
            ++blanks;
        auto commas = blanks;
        while (commas < line.size() && line[commas] == ',')
            ++commas;
        const auto rest = line.substr(commas);
        if (commas > blanks &&
            (rest.starts_with('*') || rest.starts_with("#+"))) {
            out.append(line.substr(0, blanks));
            out.append(line.substr(blanks + 1));
        } else {
            out.append(line);
        }
    }
    return out;
}

/**
 * @brief The column the leading blanks of @p line reach and the bytes they
 * take.
 */
std::pair<std::size_t, std::size_t> indentation(std::string_view line) {
    std::size_t column = 0;
    std::size_t i = 0;
    for (; i < line.size() && is_blank(line[i]); ++i)
        column = line[i] == '\t' ? (column / tab_width + 1) * tab_width
                                 : column + 1;
    return {column, i};
}

/**
 * @brief @p line without its first @p n columns. A tab that crosses column
 * n stays: move-to-column puts spaces before it, and only those go.
 */
std::string_view after_columns(std::string_view line, std::size_t n) {
    std::size_t column = 0;
    std::size_t i = 0;
    while (column < n) {
        if (line[i] == '\t') {
            const auto next = (column / tab_width + 1) * tab_width;
            if (next > n)
                break;
            column = next;
        } else {
            ++column;
        }
        ++i;
    }
    return line.substr(i);
}

/**
 * @brief org-remove-indentation: @p code without the indentation common to
 * its non-blank lines, or @p code itself when that fails.
 */
std::string remove_indentation(std::string_view code) {
    auto lines = split(code, '\n');
    std::optional<std::size_t> least;
    for (const auto line : lines) {
        const auto [column, end] = indentation(line);
        if (end == line.size() || is_space(line[end]))
            continue;
        if (column == 0)
            return std::string(code);
        least = std::min(least.value_or(column), column);
    }
    std::string out;
    out.reserve(code.size());
    for (std::size_t k = 0; k < lines.size(); ++k) {
        if (k > 0)
            out.push_back('\n');
        const auto line = lines[k];
        const auto [column, end] = indentation(line);
        if (least && column >= *least)
            out.append(after_columns(line, *least));
        else if (end != line.size())
            return std::string(code);
    }
    return out;
}

std::string_view trim(std::string_view s) {
    while (!s.empty() && is_trimmed(s.front()))
        s.remove_prefix(1);
    while (!s.empty() && is_trimmed(s.back()))
        s.remove_suffix(1);
    return s;
}

/**
 * @brief One header argument of an opening line: its name without the
 * colon and its value, the words up to the next argument.
 */
struct argument {
    std::string_view name;
    std::string value;
};

/**
 * @brief The words before the first header argument, which org reads as
 * switches, and the header arguments.
 */
struct parameters {
    std::vector<std::string_view> switches;
    std::vector<argument> arguments;
};

parameters parse_parameters(std::string_view text) {
    parameters p;
    for (const auto word : split(text, ' ')) {
        for (const auto part : split(word, '\t')) {
            if (part.empty())
                continue;
            if (part.starts_with(':')) {
                p.arguments.push_back({part.substr(1), {}});
            } else if (p.arguments.empty()) {
                p.switches.push_back(part);
            } else {
                auto& value = p.arguments.back().value;
                if (!value.empty())
                    value.push_back(' ');
                value.append(part);
            }
        }
    }
    return p;
}

bool is_accepted(const argument& a) {
    return std::ranges::any_of(accepted_arguments,
        [&](const accepted_argument& known) {
            return known.name == a.name &&
                (known.value.empty() || known.value == a.value);
        });
}

bool is_plain(std::string_view value) {
    return !value.empty() && value != "yes" &&
        value.find(' ') == std::string_view::npos &&
        std::string_view("\"'(`").find(value.front()) ==
            std::string_view::npos;
}

std::string_view directory_of(std::string_view path) {
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string_view{}
                                           : path.substr(0, slash);
}

/**
 * @brief @p target resolved against @p directory, as a path relative to the
 * root of the checkout, or nothing when it is absolute or leaves the root.
 */
std::optional<std::string> resolve(std::string_view directory,
    std::string_view target) {
    if (target.starts_with('/') || target.starts_with('~'))
        return std::nullopt;
    std::vector<std::string_view> parts;
    for (const auto path : {directory, target}) {
        for (const auto part : split(path, '/')) {
            if (part.empty() || part == ".")
                continue;
            if (part != "..")
                parts.push_back(part);
            else if (parts.empty())
                return std::nullopt;
            else
                parts.pop_back();
        }
    }
    if (parts.empty())
        return std::nullopt;
    std::string out;
    for (const auto part : parts) {
        if (!out.empty())
            out.push_back('/');
        out.append(part);
    }
    return out;
}

bool has_tag(const types::document& d, ot::heading_id h,
    std::string_view tag) {
    const auto& headings = d.tables.headings;
    const auto first = headings.tag_firsts[h].value;
    const auto end = h.next().value < headings.size()
        ? headings.tag_firsts[h.next()].value
        : static_cast<std::uint32_t>(d.tables.heading_tags.size());
    for (auto t = first; t < end; ++t) {
        const auto name = d.tables.heading_tags.names[ot::tag_id{t}];
        if (org::ops::name_text(d.tables, name) == tag)
            return true;
    }
    return false;
}

/**
 * @brief True when element @p e is under a COMMENT heading or a heading
 * tagged ARCHIVE, at any depth: org-babel-tangle skips those blocks.
 */
bool is_skipped(const types::document& d, ot::element_id e) {
    const auto& elements = d.tables.elements;
    const auto headings = d.tables.headings.elements.values;
    for (auto at = elements.parents[e]; !at.is_none() && at.value != 0;
        at = elements.parents[at]) {
        if (elements.kinds[at] != ot::element_kind::heading)
            continue;
        const auto it = std::ranges::lower_bound(headings, at);
        const ot::heading_id h{
            static_cast<std::uint32_t>(it - headings.begin())};
        if (d.tables.headings.comments[h] == ot::comment_state::commented ||
            has_tag(d, h, "ARCHIVE"))
            return true;
    }
    return false;
}

bool has_header_line(const types::document& d, ot::element_id e) {
    const auto& affiliated = d.tables.affiliated;
    const auto owners = affiliated.owners.values;
    for (auto it = std::ranges::lower_bound(owners, e);
        it != owners.end() && *it == e; ++it) {
        const ot::affiliated_id a{
            static_cast<std::uint32_t>(it - owners.begin())};
        if (org::ops::name_text(d.tables, affiliated.names[a]) == "header")
            return true;
    }
    return false;
}

bool starts_with_folded(std::string_view text, std::string_view prefix) {
    const auto lower = [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    };
    return text.size() >= prefix.size() &&
        std::ranges::equal(text.substr(0, prefix.size()), prefix, {}, lower,
            lower);
}

/**
 * @brief The places outside opening lines that set header arguments: a
 * #+property: keyword or a property drawer entry named header-args.
 */
std::vector<ot::source_range> header_arg_settings(const types::document& d) {
    std::vector<ot::source_range> found;
    const auto& keywords = d.tables.keywords;
    for (std::uint32_t k = 0; k < keywords.size(); ++k) {
        const ot::keyword_id id{k};
        const auto value = org::ops::trimmed(d.source, keywords.values[id]);
        if (org::ops::name_text(d.tables, keywords.names[id]) == "property" &&
            starts_with_folded(value.of(d.source), "header-args"))
            found.push_back(value);
    }
    const auto& properties = d.tables.properties;
    for (std::uint32_t p = 0; p < properties.size(); ++p) {
        const ot::property_id id{p};
        if (org::ops::name_text(d.tables, properties.names[id])
                .starts_with("HEADER-ARGS"))
            found.push_back(properties.values[id]);
    }
    return found;
}

/**
 * @brief A target as it is assembled: the bytes of its blocks so far and
 * whether one of them has a problem.
 */
struct pending_target {
    std::string path;
    std::string content;
    bool blocked = false;
};

class tangler final {
public:
    explicit tangler(const types::document& d) : d_(d) {}

    tangled_document run() && {
        for (std::uint32_t b = 0; b < d_.tables.blocks.size(); ++b)
            add_block(ot::block_id{b});
        if (tangles_any_)
            add_document_settings();
        tangled_document out;
        out.problems = std::move(problems_);
        for (auto& t : targets_) {
            if (!t.blocked)
                out.targets.push_back({std::move(t.path),
                    std::move(t.content)});
        }
        return out;
    }

private:
    void add_block(ot::block_id b) {
        const auto& blocks = d_.tables.blocks;
        const auto element = blocks.elements[b];
        if (d_.tables.elements.kinds[element] != ot::element_kind::src_block ||
            blocks.names[b].is_none())
            return;
        const auto range = blocks.parameters[b];
        const auto p = parse_parameters(range.of(d_.source));
        const auto tangle = std::ranges::find(p.arguments, "tangle",
            &argument::name);
        if (tangle == p.arguments.end() || tangle->value == "no" ||
            is_skipped(d_, element))
            return;
        tangles_any_ = true;

        bool blocked = false;
        const auto block_problem = [&](tangle_fault fault,
            std::string message, std::string fix) {
            problems_.push_back({range, fault, std::move(message),
                std::move(fix)});
            blocked = true;
        };
        const auto& value = tangle->value;
        std::optional<std::string> path;
        if (std::ranges::count(p.arguments, "tangle", &argument::name) > 1)
            block_problem(tangle_fault::unsupported,
                "source block sets :tangle more than once",
                "Keep one :tangle argument on the block.");
        else if (!is_plain(value))
            block_problem(tangle_fault::unsupported,
                std::format("source block sets :tangle {}, which is not a "
                    "plain path", value.empty() ? "with no value" : value),
                "Name the target by a path relative to the document, "
                "with no spaces or quotes.");
        else if (path = resolve(directory_of(d_.path), value); !path)
            block_problem(tangle_fault::outside_checkout,
                std::format("source block tangles to {}, which is outside "
                    "the checkout", value),
                "Name a target under the root of the checkout, relative "
                "to the document.");

        const auto target = path.value_or(value);
        for (const auto s : p.switches)
            block_problem(tangle_fault::unsupported,
                std::format("source block tangling to {} has the switch {}, "
                    "which tangling does not implement", target, s),
                std::format("Remove {} from the block.", s));
        for (const auto& a : p.arguments) {
            if (a.name == "tangle" || is_accepted(a))
                continue;
            block_problem(tangle_fault::unsupported,
                std::format("source block tangling to {} sets :{} {}, which "
                    "tangling does not implement", target, a.name, a.value),
                std::format("Remove :{} from the block.", a.name));
        }
        if (has_header_line(d_, element))
            block_problem(tangle_fault::unsupported,
                std::format("source block tangling to {} has a #+header: "
                    "line, which tangling does not read", target),
                "Move the header arguments to the #+begin_src line.");

        if (path)
            append(*path, tangle_body(blocks.bodies[b].of(d_.source)),
                blocked);
    }

    /**
     * @brief Adds a body to its target: an empty line before it unless it
     * is the first, a newline after it.
     */
    void append(const std::string& path, const std::string& body,
        bool blocked) {
        auto t = std::ranges::find(targets_, path, &pending_target::path);
        if (t == targets_.end()) {
            targets_.push_back({path, {}, false});
            t = std::prev(targets_.end());
        } else {
            t->content.push_back('\n');
        }
        t->content += body;
        t->content.push_back('\n');
        t->blocked = t->blocked || blocked;
    }

    void add_document_settings() {
        for (const auto range : header_arg_settings(d_)) {
            problems_.push_back({range, tangle_fault::unsupported,
                "document sets header-args, which tangling does not read",
                "Set the header arguments on each #+begin_src line."});
            for (auto& t : targets_)
                t.blocked = true;
        }
    }

    const types::document& d_;
    bool tangles_any_ = false;
    std::vector<pending_target> targets_;
    std::vector<tangle_problem> problems_;
};

}

std::string tangle_body(std::string_view raw) {
    auto value = unescape(raw);
    if (value.ends_with('\n'))
        value.pop_back();
    return std::string(trim(remove_indentation(value)));
}

tangled_document tangle(const types::document& d) {
    return tangler(d).run();
}

}
