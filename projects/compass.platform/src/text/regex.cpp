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
#define PCRE2_CODE_UNIT_WIDTH 8
#include "compass.platform/text/regex.hpp"

#include <array>
#include <stdexcept>
#include <pcre2.h>

namespace compass::platform::text {

namespace {

constexpr std::size_t jit_stack_bytes = 64 * 1024 * 1024;

constexpr std::uint32_t most_groups = 256;

/**
 * @brief What one thread matches with: a JIT stack grown on demand up to
 * the limit, so a lazy loop over a large file does not exhaust the default
 * 32 KiB, and match data for any pattern, so a match allocates nothing.
 */
struct thread_state {
    pcre2_jit_stack* stack = pcre2_jit_stack_create(32 * 1024,
        jit_stack_bytes, nullptr);
    pcre2_match_context* context = pcre2_match_context_create(nullptr);
    pcre2_match_data* data = pcre2_match_data_create(most_groups, nullptr);

    thread_state() { pcre2_jit_stack_assign(context, nullptr, stack); }

    ~thread_state() {
        pcre2_match_data_free(data);
        pcre2_match_context_free(context);
        pcre2_jit_stack_free(stack);
    }

    thread_state(const thread_state&) = delete;
    thread_state& operator=(const thread_state&) = delete;
};

thread_state& state() {
    thread_local thread_state s;
    return s;
}

std::string error_text(int code) {
    std::array<PCRE2_UCHAR, 256> buffer{};
    pcre2_get_error_message(code, buffer.data(), buffer.size());
    return reinterpret_cast<const char*>(buffer.data());
}

std::size_t next_character(std::string_view s, std::size_t i) noexcept {
    ++i;
    while (i < s.size() &&
        (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80)
        ++i;
    return i;
}

}

struct regex::impl {
    pcre2_code* code = nullptr;
    std::vector<std::string> names;
    std::uint32_t capture_count = 0;

    ~impl() { pcre2_code_free(code); }
};

bool regex_match::matched(std::size_t group) const noexcept {
    return offsets_[2 * group] != PCRE2_UNSET;
}

std::string_view regex_match::group(std::size_t group) const noexcept {
    if (!matched(group))
        return {};
    return subject_.substr(start(group), end(group) - start(group));
}

std::optional<std::string_view> regex_match::group_if(std::size_t group) const
    noexcept {
    if (!matched(group))
        return std::nullopt;
    return this->group(group);
}

std::size_t regex_match::index_of(std::string_view name) const {
    for (std::size_t i = 0; i < names_->size(); ++i) {
        if ((*names_)[i] == name)
            return i;
    }
    throw std::out_of_range("no group named " + std::string(name));
}

regex::regex(std::string_view pattern, regex_flags flags)
    : impl_(std::make_unique<impl>()) {
    std::uint32_t options = PCRE2_UTF | PCRE2_UCP | PCRE2_MATCH_INVALID_UTF;
    if (flags.dotall)
        options |= PCRE2_DOTALL;
    if (flags.multiline)
        options |= PCRE2_MULTILINE;
    if (flags.ignorecase)
        options |= PCRE2_CASELESS;
    if (flags.verbose)
        options |= PCRE2_EXTENDED;
    int error = 0;
    PCRE2_SIZE at = 0;
    impl_->code = pcre2_compile(
        reinterpret_cast<PCRE2_SPTR>(pattern.data()), pattern.size(),
        options, &error, &at, nullptr);
    if (impl_->code == nullptr)
        throw std::invalid_argument("regex " + std::string(pattern) +
            " does not compile at " + std::to_string(at) + ": " +
            error_text(error));
    pcre2_jit_compile(impl_->code, PCRE2_JIT_COMPLETE);
    pcre2_pattern_info(impl_->code, PCRE2_INFO_CAPTURECOUNT,
        &impl_->capture_count);
    if (impl_->capture_count >= most_groups)
        throw std::invalid_argument("regex " + std::string(pattern) +
            " has more than " + std::to_string(most_groups - 1) + " groups");

    std::vector<std::string> names(impl_->capture_count + 1);
    std::uint32_t count = 0;
    std::uint32_t entry_size = 0;
    PCRE2_SPTR table = nullptr;
    pcre2_pattern_info(impl_->code, PCRE2_INFO_NAMECOUNT, &count);
    pcre2_pattern_info(impl_->code, PCRE2_INFO_NAMEENTRYSIZE, &entry_size);
    pcre2_pattern_info(impl_->code, PCRE2_INFO_NAMETABLE, &table);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* entry = table + i * entry_size;
        const auto number = (static_cast<std::size_t>(entry[0]) << 8) |
            entry[1];
        names[number] = reinterpret_cast<const char*>(entry + 2);
    }
    impl_->names = std::move(names);
}

regex::~regex() = default;
regex::regex(regex&&) noexcept = default;
regex& regex::operator=(regex&&) noexcept = default;

std::optional<regex_match> regex::run(std::string_view subject,
    std::size_t from, std::uint32_t options) const {
    auto& s = state();
    // Compiled with PCRE2_MATCH_INVALID_UTF, a subject needs no check.
    const auto rc = pcre2_match(impl_->code,
        reinterpret_cast<PCRE2_SPTR>(subject.data()), subject.size(), from,
        options | PCRE2_NO_UTF_CHECK, s.data, s.context);
    std::optional<regex_match> out;
    if (rc > 0) {
        const auto* ovector = pcre2_get_ovector_pointer(s.data);
        std::vector<std::size_t> offsets(ovector,
            ovector + 2 * (impl_->capture_count + 1));
        out.emplace(subject, std::move(offsets), &impl_->names);
    }
    if (rc < 0 && rc != PCRE2_ERROR_NOMATCH)
        throw std::runtime_error("regex match failed: " + error_text(rc));
    return out;
}

std::optional<regex_match> regex::search(std::string_view subject,
    std::size_t from) const {
    return run(subject, from, 0);
}

std::optional<regex_match> regex::match(std::string_view subject,
    std::size_t from) const {
    return run(subject, from, PCRE2_ANCHORED);
}

std::optional<regex_match> regex::fullmatch(std::string_view subject) const {
    return run(subject, 0, PCRE2_ANCHORED | PCRE2_ENDANCHORED);
}

std::vector<regex_match> regex::find_all(std::string_view subject) const {
    std::vector<regex_match> out;
    std::size_t from = 0;
    std::uint32_t options = 0;
    while (from <= subject.size()) {
        auto m = run(subject, from, options);
        if (!m) {
            if (options == 0)
                break;
            from = next_character(subject, from);
            options = 0;
            continue;
        }
        const auto end = m->end();
        const auto empty = m->start() == end;
        out.push_back(std::move(*m));
        from = end;
        options = empty ? PCRE2_NOTEMPTY_ATSTART | PCRE2_ANCHORED : 0;
    }
    return out;
}

std::string regex::substitute(std::string_view subject,
    const std::function<std::string(const regex_match&)>& with) const {
    std::string out;
    std::size_t copied = 0;
    for (const auto& m : find_all(subject)) {
        out.append(subject.substr(copied, m.start() - copied));
        out += with(m);
        copied = m.end();
    }
    out.append(subject.substr(copied));
    return out;
}

std::string regex::substitute(std::string_view subject,
    std::string_view with) const {
    return substitute(subject,
        [&](const regex_match&) { return std::string(with); });
}

std::string escape(std::string_view text) {
    std::string out;
    out.reserve(text.size() * 2);
    for (const char c : text) {
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' ||
            static_cast<unsigned char>(c) >= 0x80;
        if (!plain)
            out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

std::string blank_keeping_newlines(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c == '\n')
            out.push_back('\n');
        else if ((static_cast<unsigned char>(c) & 0xC0) != 0x80)
            out.push_back(' ');
    }
    return out;
}

}
