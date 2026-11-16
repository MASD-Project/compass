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
#include "compass.store/ops/git_settings.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace compass::store::ops {

namespace {

constexpr std::array<std::string_view, 6> converting_attributes{"text", "eol",
    "crlf", "filter", "ident", "working-tree-encoding"};

bool is_blank(char c) noexcept { return c == ' ' || c == '\t' || c == '\r'; }

std::string lower(std::string_view s) {
    std::string r(s);
    std::ranges::transform(r, r.begin(), [](char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    });
    return r;
}

/**
 * @brief Parses git configuration text one item at a time: a section
 * header or a key with its value.
 */
class config_reader final {
public:
    explicit config_reader(std::string_view text) noexcept : text_(text) {}

    void run(types::git_config& config) {
        while (skip_blanks(), at_ < text_.size()) {
            const char c = text_[at_];
            if (c == '\n') {
                ++at_;
            } else if (c == '#' || c == ';') {
                skip_line();
            } else if (c == '[') {
                read_header();
            } else {
                read_entry(config);
            }
        }
    }

private:
    void skip_blanks() noexcept {
        while (at_ < text_.size() && is_blank(text_[at_]))
            ++at_;
    }

    void skip_line() noexcept {
        const auto nl = text_.find('\n', at_);
        at_ = nl == std::string_view::npos ? text_.size() : nl + 1;
    }

    void read_header() {
        const auto close = text_.find(']', at_);
        if (close == std::string_view::npos) {
            at_ = text_.size();
            return;
        }
        const auto header = text_.substr(at_ + 1, close - at_ - 1);
        const auto end = header.find_first_of(" \t\".");
        section_ = lower(header.substr(0, end));
        at_ = close + 1;
    }

    void read_entry(types::git_config& config) {
        const auto start = at_;
        while (at_ < text_.size() && (std::isalnum(
            static_cast<unsigned char>(text_[at_])) || text_[at_] == '-'))
            ++at_;
        const auto key = lower(text_.substr(start, at_ - start));
        if (key.empty()) {
            skip_line();
            return;
        }
        skip_blanks();
        std::optional<std::string> value;
        if (at_ < text_.size() && text_[at_] == '=') {
            ++at_;
            value = read_value();
        } else {
            skip_line();
        }
        apply(key, value, config);
    }

    std::string read_value() {
        skip_blanks();
        std::string v;
        std::size_t kept = 0;
        bool quoted = false;
        while (at_ < text_.size()) {
            const char c = text_[at_];
            if (c == '\n')
                break;
            if (c == '\\' && at_ + 1 < text_.size()) {
                const char next = text_[at_ + 1];
                at_ += 2;
                if (next == '\n')
                    continue;
                if (next == '\r' && at_ < text_.size() && text_[at_] == '\n') {
                    ++at_;
                    continue;
                }
                v += next == 'n' ? '\n' : next == 't' ? '\t' : next;
                kept = v.size();
                continue;
            }
            ++at_;
            if (c == '"') {
                quoted = !quoted;
                kept = v.size();
            } else if (!quoted && (c == '#' || c == ';')) {
                skip_line();
                break;
            } else {
                v += c;
                if (quoted || !is_blank(c))
                    kept = v.size();
            }
        }
        v.resize(kept);
        return v;
    }

    void apply(const std::string& key, const std::optional<std::string>& value,
        types::git_config& config) const {
        const auto v = value ? lower(*value) : std::string("true");
        if (section_ == "core" && key == "autocrlf") {
            if (v != "false" && v != "no" && v != "off" && v != "0" &&
                !v.empty())
                config.autocrlf = true;
        } else if (section_ == "core" && key == "attributesfile" && value) {
            config.attributes_files.push_back(*value);
        } else if (section_ == "extensions" && key == "objectformat") {
            if (v != "sha1")
                config.sha1 = false;
        } else if ((section_ == "include" || section_ == "includeif") &&
            key == "path" && value) {
            config.includes.push_back(*value);
        }
    }

    std::string_view text_;
    std::size_t at_ = 0;
    std::string section_;
};

std::vector<std::string_view> split_blanks(std::string_view s) {
    std::vector<std::string_view> r;
    std::size_t at = 0;
    while (at < s.size()) {
        while (at < s.size() && is_blank(s[at]))
            ++at;
        const auto start = at;
        while (at < s.size() && !is_blank(s[at]))
            ++at;
        if (at > start)
            r.push_back(s.substr(start, at - start));
    }
    return r;
}

/**
 * @brief Splits an attributes line into its pattern, unquoted when git's
 * C-style quoting is used, and the rest.
 */
std::pair<std::string, std::string_view> split_pattern(std::string_view line) {
    if (line.front() != '"') {
        const auto end = std::ranges::find_if(line, is_blank) - line.begin();
        return {std::string(line.substr(0, end)), line.substr(end)};
    }
    std::string pattern;
    std::size_t at = 1;
    while (at < line.size() && line[at] != '"') {
        if (line[at] == '\\' && at + 1 < line.size()) {
            const char next = line[at + 1];
            pattern += next == 'n' ? '\n' : next == 't' ? '\t' : next;
            at += 2;
        } else {
            pattern += line[at++];
        }
    }
    return {pattern, line.substr(std::min(at + 1, line.size()))};
}

bool sets_conversion(std::string_view rest,
    const std::vector<std::string>& macros) {
    for (const auto token : split_blanks(rest)) {
        if (token.front() == '-' || token.front() == '!')
            continue;
        const auto name = token.substr(0, token.find('='));
        if (std::ranges::find(converting_attributes, name) !=
                converting_attributes.end() ||
            std::ranges::find(macros, name) != macros.end())
            return true;
    }
    return false;
}

/**
 * @brief Matches @p text from @p ti against @p pattern from @p pi, the way
 * git's wildmatch does with WM_PATHNAME.
 */
bool wildmatch(std::string_view pattern, std::size_t pi, std::string_view text,
    std::size_t ti) {
    while (pi < pattern.size()) {
        const char c = pattern[pi];
        if (c == '*') {
            const bool doubled = pi + 1 < pattern.size() &&
                pattern[pi + 1] == '*' && (pi == 0 || pattern[pi - 1] == '/') &&
                (pi + 2 == pattern.size() || pattern[pi + 2] == '/');
            if (doubled) {
                if (pi + 2 == pattern.size())
                    return true;
                for (auto t = ti;;) {
                    if (wildmatch(pattern, pi + 3, text, t))
                        return true;
                    const auto slash = text.find('/', t);
                    if (slash == std::string_view::npos)
                        return false;
                    t = slash + 1;
                }
            }
            while (pi < pattern.size() && pattern[pi] == '*')
                ++pi;
            for (auto t = ti;; ++t) {
                if (wildmatch(pattern, pi, text, t))
                    return true;
                if (t == text.size() || text[t] == '/')
                    return false;
            }
        }
        if (ti == text.size())
            return false;
        const char t = text[ti];
        if (c == '?') {
            if (t == '/')
                return false;
        } else if (c == '[') {
            auto at = pi + 1;
            const bool negated = at < pattern.size() &&
                (pattern[at] == '!' || pattern[at] == '^');
            if (negated)
                ++at;
            bool matched = false;
            for (bool first = true; at < pattern.size() &&
                (first || pattern[at] != ']'); first = false) {
                char low = pattern[at];
                if (low == '\\' && at + 1 < pattern.size())
                    low = pattern[++at];
                char high = low;
                if (at + 2 < pattern.size() && pattern[at + 1] == '-' &&
                    pattern[at + 2] != ']') {
                    high = pattern[at + 2];
                    if (high == '\\' && at + 3 < pattern.size())
                        high = pattern[++at + 2];
                    at += 2;
                }
                matched = matched || (t >= low && t <= high);
                ++at;
            }
            if (at >= pattern.size())
                return true;
            if (t == '/' || matched == negated)
                return false;
            pi = at;
        } else {
            const char literal =
                c == '\\' && pi + 1 < pattern.size() ? pattern[++pi] : c;
            if (literal != t)
                return false;
        }
        ++pi;
        ++ti;
    }
    return ti == text.size();
}

}

void read_git_config(std::string_view text, types::git_config& config) {
    config_reader(text).run(config);
}

void read_attributes(std::string_view text, std::string_view base,
    types::attribute_rules& rules) {
    std::size_t at = 0;
    while (at < text.size()) {
        const auto nl = text.find('\n', at);
        auto line = text.substr(at, nl == std::string_view::npos
            ? std::string_view::npos : nl - at);
        at = nl == std::string_view::npos ? text.size() : nl + 1;

        while (!line.empty() && is_blank(line.front()))
            line.remove_prefix(1);
        if (line.empty() || line.front() == '#')
            continue;
        auto [pattern, rest] = split_pattern(line);
        if (!sets_conversion(rest, rules.converting_macros))
            continue;
        if (pattern.starts_with("[attr]")) {
            rules.converting_macros.push_back(pattern.substr(6));
            continue;
        }
        if (pattern.empty() || pattern.front() == '!' || pattern.back() == '/')
            continue;
        if (pattern.find("[:") != std::string::npos)
            pattern = "**";
        rules.bases.emplace_back(base);
        rules.patterns.push_back(std::move(pattern));
    }
}

bool converts(const types::attribute_rules& rules, std::string_view path) {
    for (std::size_t i = 0; i < rules.patterns.size(); ++i) {
        if (!path.starts_with(rules.bases[i]))
            continue;
        const auto relative = path.substr(rules.bases[i].size());
        std::string_view pattern = rules.patterns[i];
        if (pattern.find('/') == std::string_view::npos) {
            const auto slash = relative.rfind('/');
            const auto name = slash == std::string_view::npos
                ? relative : relative.substr(slash + 1);
            if (wildmatch(pattern, 0, name, 0))
                return true;
        } else {
            if (pattern.front() == '/')
                pattern.remove_prefix(1);
            if (wildmatch(pattern, 0, relative, 0))
                return true;
        }
    }
    return false;
}

}
