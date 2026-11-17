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
#include "compass.docs/codecs/recipe_libraries.hpp"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "compass.codec/ops/tangle.hpp"
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::docs::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

/**
 * @brief What tells the two Emacs generators apart: the language they
 * tangle, the filetag kind that names a script's folder, the suffix of the
 * one script a recipe that names no target gives (none: such a recipe
 * gives nothing), and how a script is finished.
 */
struct library_style {
    std::string_view language;
    std::string_view kind;
    std::string_view build_target;
    std::string_view whole_document_suffix;
    bool strips_exit = false;
    bool describes = false;
};

constexpr library_style shell_style{"ores-shell", "shell",
    "tangle_shell_scripts", ".ores", true, true};
constexpr library_style http_style{"hurl", "http", "tangle_http_recipes",
    "", false, false};

/**
 * @brief string-trim: blanks and line ends off both ends.
 */
std::string_view trim(std::string_view s) {
    constexpr std::string_view blanks = " \t\n\r";
    const auto first = s.find_first_not_of(blanks);
    if (first == std::string_view::npos)
        return {};
    return s.substr(first, s.find_last_not_of(blanks) - first + 1);
}

/**
 * @brief The first #+KEY: value, matched without regard to case as
 * re-search-forward does under case-fold-search, and trimmed.
 */
std::optional<std::string_view> keyword(std::string_view text,
    std::string_view key) {
    const pt::regex line("^#\\+" + pt::escape(key) + ":[ \\t]*(.*)$",
        {.multiline = true, .ignorecase = true});
    const auto m = line.search(text);
    if (!m)
        return std::nullopt;
    return trim(m->group(1));
}

std::string opening_pattern(const library_style& s) {
    return "^[ \\t]*#\\+begin_src[ \\t]+" + pt::escape(s.language) +
        "\\b[^\\n]*";
}

/**
 * @brief The :tangle value of each opening line of the style's language,
 * in document order, as the generator's own scan reads them.
 */
std::vector<std::string_view> block_tangles(std::string_view text,
    const library_style& s) {
    const pt::regex opening(opening_pattern(s),
        {.multiline = true, .ignorecase = true});
    static const pt::regex tangle(":tangle[ \\t]+([^ \\t\\n]+)",
        {.ignorecase = true});
    std::vector<std::string_view> out;
    for (const auto& m : opening.find_all(text)) {
        if (const auto t = tangle.search(m.group()))
            out.push_back(t->group(1));
    }
    return out;
}

/**
 * @brief The heading of the section whose opening line tangles to
 * @p target, or empty.
 */
std::string_view section_title(std::string_view text,
    const library_style& s, std::string_view target) {
    static const pt::regex heading("^\\*+ +(.*)[ \\t]*$");
    const pt::regex opening(opening_pattern(s), {.ignorecase = true});
    static const pt::regex tangle(":tangle[ \\t]+([^ \\t\\n]+)",
        {.ignorecase = true});
    std::string_view current;
    std::size_t start = 0;
    while (start < text.size()) {
        const auto end = std::min(text.find('\n', start), text.size());
        const auto line = text.substr(start, end - start);
        if (const auto h = heading.match(line)) {
            current = trim(h->group(1));
        } else if (opening.match(line)) {
            const auto t = tangle.search(line);
            if (t && t->group(1) == target)
                return current;
        }
        start = end + 1;
    }
    return {};
}

std::string category(std::string_view text, const library_style& s) {
    const auto tags = keyword(text, "filetags");
    if (tags) {
        const pt::regex tag(":recipe:" + pt::escape(s.kind) + ":([^:]+):",
            {.ignorecase = true});
        if (const auto m = tag.search(*tags))
            return std::string(m->group(1));
    }
    return "general";
}

/**
 * @brief The script without a last line that is only exit, and without
 * the blanks and newlines that end it, then one newline.
 */
std::string strip_trailing_exit(std::string_view script) {
    constexpr std::string_view trailing = " \t\n";
    const auto rtrim = [&](std::string_view t) {
        const auto last = t.find_last_not_of(trailing);
        return last == std::string_view::npos ? std::string_view{}
                                              : t.substr(0, last + 1);
    };
    auto body = rtrim(script);
    const auto newline = body.rfind('\n');
    const auto line_start = newline == std::string_view::npos ? 0
                                                              : newline + 1;
    static const pt::regex exit_line("[ \\t]*exit[ \\t]*");
    if (exit_line.fullmatch(body.substr(line_start)))
        body = rtrim(body.substr(0, line_start));
    return std::string(body) + "\n";
}

std::string banner(std::string_view path, std::string_view text,
    const library_style& s, std::optional<std::string_view> section) {
    const auto title = keyword(text, "title");
    auto description = keyword(text, "description");
    if (description && (description->empty() || description == title))
        description.reset();
    std::string out;
    if (section && !section->empty())
        out += "# " + std::string(*section) + "\n";
    else if (title && !title->empty())
        out += "# " + std::string(*title) + "\n";
    if (s.describes && !section && description)
        out += "# " + std::string(*description) + "\n";
    out += "#\n# GENERATED from " + std::string(path) +
        " \xe2\x80\x94 do not edit by hand.\n"
        "# Regenerate with: ./compass.sh build --direct " +
        std::string(s.build_target) + "\n#\n";
    return out;
}

std::string finish(std::string_view script, std::string_view path,
    std::string_view text, const library_style& s,
    std::optional<std::string_view> section) {
    return banner(path, text, s, section) +
        (s.strips_exit ? strip_trailing_exit(script) : std::string(script));
}

std::string join(std::string_view directory, std::string_view name) {
    return std::string(directory) + "/" + std::string(name);
}

std::string_view file_name(std::string_view path) {
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

/**
 * @brief The path of @p raw resolved against @p directory as
 * expand-file-name resolves it, relative to the checkout.
 */
std::string resolve(std::string_view directory, std::string_view raw) {
    std::vector<std::string_view> parts;
    for (const auto path : {directory, raw}) {
        for (const auto part : co::components_of(path)) {
            if (part.empty() || part == ".")
                continue;
            if (part != "..")
                parts.push_back(part);
            else if (!parts.empty())
                parts.pop_back();
        }
    }
    std::string out;
    for (const auto part : parts)
        out += (out.empty() ? "" : "/") + std::string(part);
    return out;
}

void generate(const ct::document& d, const ct::corpus_view& v,
    ct::file_table& out, const library_style& s) {
    const auto source = v.option("source");
    if (!d.path.starts_with(std::string(source) + "/") ||
        !d.path.ends_with(".org"))
        return;
    const auto text = d.source;
    const pt::regex has_block(opening_pattern(s),
        {.multiline = true, .ignorecase = true});
    if (!has_block.search(text))
        return;
    const auto folder = join(v.option("library"), category(text, s));
    const auto tangles = block_tangles(text, s);
    if (tangles.empty()) {
        if (s.whole_document_suffix.empty())
            return;
        const auto target = join(folder, std::string(co::stem_of(d.path)) +
            std::string(s.whole_document_suffix));
        const auto tangled = compass::codec::ops::tangle(d,
            {.language = s.language, .default_target = target,
                .inert_variables = true});
        for (const auto& t : tangled.targets)
            out.add(t.path, finish(t.content, d.path, text, s,
                std::nullopt));
        return;
    }
    const auto tangled = compass::codec::ops::tangle(d,
        {.language = s.language, .default_target = {},
                .inert_variables = true});
    std::vector<std::string> done;
    const auto directory = co::parent_of(d.path);
    for (const auto raw : tangles) {
        const auto produced = resolve(directory, raw);
        if (std::ranges::contains(done, produced))
            continue;
        const auto t = std::ranges::find(tangled.targets, produced,
            &compass::codec::ops::tangle_target::path);
        if (t == tangled.targets.end())
            continue;
        done.push_back(produced);
        out.add(join(folder, file_name(raw)), finish(t->content, d.path,
            text, s, section_title(text, s, raw)));
    }
}

void shell(const ct::document& d, const ct::corpus_view& v,
    ct::file_table& out) {
    generate(d, v, out, shell_style);
}

void http(const ct::document& d, const ct::corpus_view& v,
    ct::file_table& out) {
    generate(d, v, out, http_style);
}

constexpr std::array<ct::option, 2> shell_options{{
    {"source", "doc/recipes/shell", "The directory of the shell recipes, "
        "read recursively."},
    {"library", "projects/ores.shell/scripts/library", "The directory the "
        "scripts are written to."}}};

constexpr std::array<ct::option, 2> http_options{{
    {"source", "doc/recipes/http", "The directory of the HTTP recipes, "
        "read recursively."},
    {"library", "projects/ores.http/scripts/library", "The directory the "
        "Hurl files are written to."}}};

}

const ct::descriptor shell_recipe_library{
    .name = "shell_recipe_library",
    .version = 1,
    .summary = "Writes the ores-shell script library the shell recipes "
        "tangle to.",
    .reads = ct::inputs::every_document(),
    .options = shell_options,
    .rules = {},
    .enabled_by_default = false,
    .run = ct::document_function<ct::file_table>{shell}};

const ct::descriptor http_recipe_library{
    .name = "http_recipe_library",
    .version = 1,
    .summary = "Writes the Hurl library the HTTP recipes tangle to.",
    .reads = ct::inputs::every_document(),
    .options = http_options,
    .rules = {},
    .enabled_by_default = false,
    .run = ct::document_function<ct::file_table>{http}};

}
