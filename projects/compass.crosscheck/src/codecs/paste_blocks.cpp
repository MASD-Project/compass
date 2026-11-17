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
#include "compass.crosscheck/codecs/paste_blocks.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "compass.codec/types/corpus_view.hpp"
#include "compass.cpp/ops/paths.hpp"
#include "compass.cpp/ops/report.hpp"
#include "compass.cpp/ops/test_case_reachability.hpp"
#include "compass.cpp/ops/text.hpp"
#include "compass.platform/text/decoding.hpp"
#include "compass.platform/text/regex.hpp"

namespace compass::crosscheck::codecs {

namespace {

namespace ct = compass::codec::types;
namespace co = compass::cpp::ops;
namespace pt = compass::platform::text;

constexpr std::uint32_t unresolved_name = 0;
constexpr std::uint32_t nothing_read = 1;

constexpr std::array<ct::rule, 2> rules{{
    {"unresolved_name", "Every ores:: name a model's :implements block "
        "pastes is declared by some header under an include directory."},
    {"nothing_read", "The tree has an :implements block, a header and a "
        "qualified name in a block to check."}}};

struct text_hash {
    using is_transparent = void;
    std::size_t operator()(std::string_view s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

using names = std::unordered_set<std::string, text_hash, std::equal_to<>>;

/**
 * @brief The (scope, identifier) pairs of the headers: per scope, its
 * parts joined by "::", the identifiers it holds.
 */
using symbol_table = std::unordered_map<std::string, names, text_hash,
    std::equal_to<>>;

names& scope_in(symbol_table& table, std::string_view scope) {
    if (const auto it = table.find(scope); it != table.end())
        return it->second;
    return table.emplace(std::string(scope), names{}).first->second;
}

void add(names& in, std::string_view identifier) {
    if (!in.contains(identifier))
        in.emplace(identifier);
}

std::string joined(const std::string& scope, std::string_view part) {
    return scope.empty() ? std::string(part) : scope + "::" + std::string(part);
}

/**
 * @brief True when a class, struct, union or enum keyword starts at @p i
 * after a word boundary, which the type pattern needs before it can match.
 */
bool type_keyword_at(std::string_view text, std::size_t i) noexcept {
    if (i > 0 && co::is_word(text[i - 1]))
        return false;
    const auto rest = text.substr(i);
    return rest.starts_with("class") || rest.starts_with("struct") ||
        rest.starts_with("union") || rest.starts_with("enum");
}

std::size_t identifier_end(std::string_view text, std::size_t i) noexcept {
    if (i >= text.size() || !(co::is_identifier(text[i]) &&
            !(text[i] >= '0' && text[i] <= '9')))
        return i;
    ++i;
    while (i < text.size() && co::is_word(text[i]))
        ++i;
    return i;
}

/**
 * @brief The length of the UTF-8 character that starts with @p c, one for
 * a byte that starts none.
 */
std::size_t character_length(char c) noexcept {
    const auto b = static_cast<unsigned char>(c);
    if (b >= 0xF0)
        return 4;
    if (b >= 0xE0)
        return 3;
    if (b >= 0xC0)
        return 2;
    return 1;
}

/**
 * @brief Adds to @p out every (scope, identifier) pair @p text mentions,
 * as the script's declared_symbols walks it: a namespace or a class,
 * struct, union or enum opens a scope at its brace, any other brace opens
 * one with the scope it is in, and a closing brace goes back.
 */
void declared_symbols(std::string_view source, symbol_table& out) {
    static const pt::regex name_space(
        R"(namespace\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*\{)");
    static const pt::regex type(
        R"(\b(?:class|struct|union|enum(?:\s+class)?)\s+(?:[A-Z_][A-Z0-9_]*\s+)*([A-Za-z_]\w*)[^;{]*\{)");
    const auto text = co::blank_comments_and_literals(source);
    std::vector<std::string> scopes;
    std::string scope;
    names* current = &scope_in(out, scope);
    const auto enter = [&](std::string next) {
        scope = std::move(next);
        current = &scope_in(out, scope);
    };
    std::size_t i = 0;
    const auto n = text.size();
    while (i < n) {
        const auto c = text[i];
        if (c == 'n' && std::string_view(text).substr(i, 9) == "namespace") {
            if (const auto m = name_space.match(text, i)) {
                const std::string names = std::string(m->group(1));
                std::size_t start = 0;
                for (;;) {
                    const auto sep = names.find("::", start);
                    const auto part = std::string_view(names).substr(start,
                        sep == std::string::npos ? std::string::npos
                                                 : sep - start);
                    add(*current, part);
                    enter(joined(scope, part));
                    if (sep == std::string::npos)
                        break;
                    start = sep + 2;
                }
                scopes.push_back(scope);
                i = m->end();
                continue;
            }
        }
        if (type_keyword_at(text, i)) {
            if (const auto m = type.match(text, i)) {
                add(*current, m->group(1));
                enter(joined(scope, m->group(1)));
                scopes.push_back(scope);
                i = m->end();
                continue;
            }
        }
        if (c == '{') {
            scopes.push_back(scope);
            ++i;
            continue;
        }
        if (c == '}') {
            if (!scopes.empty())
                scopes.pop_back();
            enter(scopes.empty() ? std::string() : scopes.back());
            ++i;
            continue;
        }
        const auto end = identifier_end(text, i);
        if (end != i) {
            add(*current, std::string_view(text).substr(i, end - i));
            i = end;
            continue;
        }
        i += character_length(c);
    }
}

/**
 * @brief An :implements block: the model it is in, the offset of its
 * header there, its :name and its body.
 */
struct block {
    std::string_view model;
    std::uint32_t offset;
    std::string name;
    std::string body;
};

bool is_model(std::string_view path) {
    const auto parts = co::components_of(path);
    return parts.size() >= 4 && parts[0] == "projects" &&
        parts[2] == "modeling" && path.ends_with(".org");
}

bool is_header(std::string_view path) {
    const auto parts = co::components_of(path);
    return std::ranges::find(parts.begin(), parts.end() - 1, "include") !=
        parts.end() - 1;
}

void run(const ct::corpus_view& v, ct::diagnostic_table& out) {
    static const pt::regex paste(
        R"(^#\+begin_src\s+cpp[^\n]*:implements[^\n]*\n(?P<body>.*?)^#\+end_src)",
        {.dotall = true, .multiline = true});
    static const pt::regex block_name(R"(:name\s+(\S+))");
    static const pt::regex qualified(
        R"(\bores(?:[_a-z0-9]*)?(?:::[A-Za-z_]\w*)+)");

    auto models = v.list_files("projects/", ".org");
    std::erase_if(models, [](auto p) { return !is_model(p); });
    std::ranges::sort(models, co::component_less);
    std::vector<block> blocks;
    for (const auto path : models) {
        const auto bytes = v.read_file(path).value_or("");
        const auto text = pt::universal_newlines(pt::decode_replacing(bytes));
        if (text.find(":implements") == std::string::npos)
            continue;
        for (const auto& m : paste.find_all(text)) {
            const auto header = std::string_view(text).substr(m.start(),
                m.start("body") - m.start());
            const auto named = block_name.search(header);
            blocks.push_back({path, co::offset_of_line(bytes,
                    co::line_of(text, m.start())),
                named ? std::string(named->group(1)) : "(unnamed)",
                std::string(m.group("body"))});
        }
    }
    if (blocks.empty()) {
        co::report(out, "projects", 0, nothing_read,
            "no :implements block found under any modeling directory",
            "the check reads the org files under projects/<component>/modeling/");
        return;
    }

    auto headers = v.list_files("projects/", ".hpp");
    std::erase_if(headers, [](auto p) { return !is_header(p); });
    if (headers.empty()) {
        co::report(out, "projects", 0, nothing_read,
            "no header found under an include directory of projects/",
            "the check reads every .hpp under an include directory");
        return;
    }
    symbol_table table;
    for (const auto path : headers) {
        const auto bytes = v.read_file(path).value_or("");
        declared_symbols(pt::universal_newlines(pt::decode_replacing(bytes)),
            table);
    }

    bool any_name = false;
    for (const auto& b : blocks) {
        std::set<std::string> names;
        for (const auto& m : qualified.find_all(b.body))
            names.emplace(m.group());
        any_name = any_name || !names.empty();
        for (const auto& name : names) {
            const auto sep = name.rfind("::");
            const auto scope = std::string_view(name).substr(0, sep);
            const auto identifier = std::string_view(name).substr(sep + 2);
            if (const auto it = table.find(scope);
                it != table.end() && it->second.contains(identifier))
                continue;
            co::report(out, b.model, b.offset, unresolved_name,
                "block " + b.name + ": " + name + " names nothing: no "
                "namespace or type " + std::string(scope) + " declares " +
                std::string(identifier),
                "Point the block at the symbol's new home, or restore the "
                "symbol; a pasted block is compiled only when its component "
                "is regenerated.");
        }
    }
    if (!any_name)
        co::report(out, "projects", 0, nothing_read,
            "no qualified name found in any :implements block; the pattern "
            "this check looks for is a name rooted at ores",
            "the check reads names of the form ores::a::b");
}

}

const ct::descriptor paste_blocks{
    .name = "paste_blocks",
    .version = 1,
    .summary = "Reports each ores:: name a model's pasted C++ block names "
        "that no header declares.",
    .reads = ct::inputs::of_types({}),
    .options = {},
    .rules = rules,
    .enabled_by_default = false,
    .run = ct::corpus_function<ct::diagnostic_table>{run}};

}
