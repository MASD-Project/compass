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
#ifndef COMPASS_PLATFORM_TEXT_REGEX_HPP
#define COMPASS_PLATFORM_TEXT_REGEX_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Regular expressions with the syntax and semantics of Python's re
 * module, over UTF-8 bytes, through PCRE2.
 *
 * Patterns are Python's: named groups (?P<name>...), back references
 * (?P=name), lazy quantifiers and look-arounds. \w, \d, \s and \b are
 * Unicode-aware, as they are on a Python str. Offsets are byte offsets. A
 * subject need not be valid UTF-8: an invalid byte matches nothing, as a
 * character Python's errors="ignore" drops would not.
 */
namespace compass::platform::text {

/**
 * @brief The flags of re.compile that the ported scripts use.
 */
struct regex_flags {
    bool dotall = false;
    bool multiline = false;
    bool ignorecase = false;
    bool verbose = false;
};

/**
 * @brief One match: the byte range of the whole match and of each group.
 * An unmatched group has no range. It refers to its subject and pattern,
 * which must outlive it.
 */
class regex_match final {
public:
    regex_match(std::string_view subject, std::vector<std::size_t> offsets,
        const std::vector<std::string>* names) noexcept
        : subject_(subject), offsets_(std::move(offsets)), names_(names) {}

    [[nodiscard]] std::size_t start(std::size_t group = 0) const noexcept {
        return offsets_[2 * group];
    }

    [[nodiscard]] std::size_t end(std::size_t group = 0) const noexcept {
        return offsets_[2 * group + 1];
    }

    [[nodiscard]] bool matched(std::size_t group) const noexcept;

    /**
     * @brief The text of @p group, empty when it did not take part.
     */
    [[nodiscard]] std::string_view group(std::size_t group = 0) const
        noexcept;

    [[nodiscard]] std::optional<std::string_view> group_if(
        std::size_t group) const noexcept;

    /**
     * @throws std::out_of_range when the pattern names no such group.
     */
    [[nodiscard]] std::size_t index_of(std::string_view name) const;

    [[nodiscard]] std::string_view group(std::string_view name) const {
        return group(index_of(name));
    }

    [[nodiscard]] std::optional<std::string_view> group_if(
        std::string_view name) const {
        return group_if(index_of(name));
    }

    [[nodiscard]] bool matched(std::string_view name) const {
        return matched(index_of(name));
    }

    [[nodiscard]] std::size_t start(std::string_view name) const {
        return start(index_of(name));
    }

    [[nodiscard]] std::size_t end(std::string_view name) const {
        return end(index_of(name));
    }

    /**
     * @brief How many groups the pattern has, the whole match not counted.
     */
    [[nodiscard]] std::size_t groups() const noexcept {
        return offsets_.size() / 2 - 1;
    }

private:
    std::string_view subject_;
    std::vector<std::size_t> offsets_;
    const std::vector<std::string>* names_;
};

/**
 * @brief A compiled pattern. Safe to share between threads.
 */
class regex final {
public:
    /**
     * @throws std::invalid_argument when @p pattern does not compile.
     */
    explicit regex(std::string_view pattern, regex_flags flags = {});
    ~regex();
    regex(regex&&) noexcept;
    regex& operator=(regex&&) noexcept;
    regex(const regex&) = delete;
    regex& operator=(const regex&) = delete;

    /**
     * @brief re.search from byte @p from; ^ and \b still see the bytes
     * before it, as Pattern.search(text, pos) does.
     */
    [[nodiscard]] std::optional<regex_match> search(std::string_view subject,
        std::size_t from = 0) const;

    /**
     * @brief re.match: a match that starts at byte @p from.
     */
    [[nodiscard]] std::optional<regex_match> match(std::string_view subject,
        std::size_t from = 0) const;

    /**
     * @brief re.fullmatch: a match of the whole of @p subject.
     */
    [[nodiscard]] std::optional<regex_match> fullmatch(
        std::string_view subject) const;

    /**
     * @brief re.finditer: every match, left to right, an empty match not
     * repeated where one ended.
     */
    [[nodiscard]] std::vector<regex_match> find_all(
        std::string_view subject) const;

    /**
     * @brief re.sub with a function: each match replaced by what @p with
     * returns for it.
     */
    [[nodiscard]] std::string substitute(std::string_view subject,
        const std::function<std::string(const regex_match&)>& with) const;

    /**
     * @brief re.sub with a fixed replacement, taken literally.
     */
    [[nodiscard]] std::string substitute(std::string_view subject,
        std::string_view with) const;

private:
    struct impl;

    [[nodiscard]] std::optional<regex_match> run(std::string_view subject,
        std::size_t from, std::uint32_t options) const;

    std::unique_ptr<impl> impl_;
};

/**
 * @brief re.escape: @p text with every character that is not a letter,
 * digit or underscore escaped.
 */
[[nodiscard]] std::string escape(std::string_view text);

/**
 * @brief @p text with every byte but a newline turned into a blank, as
 * re.sub(r"[^\n]", " ", text) blanks one character.
 */
[[nodiscard]] std::string blank_keeping_newlines(std::string_view text);

}

#endif
