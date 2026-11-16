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
#include "compass.bench/io/compile_commands.hpp"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

namespace compass::bench::io {

namespace {

class json_reader final {
public:
    explicit json_reader(std::string_view text) : text_(text) {}

    std::vector<types::compile_command> read_database() {
        std::vector<types::compile_command> r;
        expect('[');
        if (!consume(']')) {
            do {
                r.push_back(read_entry());
            } while (consume(','));
            expect(']');
        }
        skip_blanks();
        if (pos_ != text_.size())
            fail("trailing data");
        return r;
    }

private:
    types::compile_command read_entry() {
        types::compile_command c;
        std::optional<std::string> command;
        bool has_arguments = false;
        expect('{');
        if (!consume('}')) {
            do {
                const auto key = read_string();
                expect(':');
                if (key == "directory") {
                    c.directory = read_string();
                } else if (key == "file") {
                    c.file = read_string();
                } else if (key == "command") {
                    command = read_string();
                } else if (key == "arguments") {
                    has_arguments = true;
                    expect('[');
                    if (!consume(']')) {
                        do {
                            c.arguments.push_back(read_string());
                        } while (consume(','));
                        expect(']');
                    }
                } else {
                    skip_value();
                }
            } while (consume(','));
            expect('}');
        }
        if (c.directory.empty() || c.file.empty() ||
            (!has_arguments && !command))
            fail("entry lacks a directory, a file or a command");
        if (!has_arguments)
            c.arguments = split_command(*command);
        const std::filesystem::path file(c.file);
        if (file.is_relative())
            c.file = (std::filesystem::path(c.directory) / file).string();
        return c;
    }

    void skip_value() {
        skip_blanks();
        if (pos_ >= text_.size())
            fail("unexpected end");
        const char c = text_[pos_];
        if (c == '"') {
            (void)read_string();
        } else if (c == '[' || c == '{') {
            const char close = c == '[' ? ']' : '}';
            ++pos_;
            if (consume(close))
                return;
            do {
                if (c == '{') {
                    (void)read_string();
                    expect(':');
                }
                skip_value();
            } while (consume(','));
            expect(close);
        } else {
            const auto start = pos_;
            while (pos_ < text_.size() &&
                (std::isalnum(static_cast<unsigned char>(text_[pos_])) ||
                    text_[pos_] == '-' || text_[pos_] == '+' ||
                    text_[pos_] == '.'))
                ++pos_;
            if (pos_ == start)
                fail("unexpected character");
        }
    }

    std::string read_string() {
        expect('"');
        std::string r;
        for (;;) {
            if (pos_ >= text_.size())
                fail("unterminated string");
            const char c = text_[pos_++];
            if (c == '"')
                return r;
            if (c != '\\') {
                r.push_back(c);
                continue;
            }
            if (pos_ >= text_.size())
                fail("unterminated escape");
            const char e = text_[pos_++];
            switch (e) {
            case '"': r.push_back('"'); break;
            case '\\': r.push_back('\\'); break;
            case '/': r.push_back('/'); break;
            case 'b': r.push_back('\b'); break;
            case 'f': r.push_back('\f'); break;
            case 'n': r.push_back('\n'); break;
            case 'r': r.push_back('\r'); break;
            case 't': r.push_back('\t'); break;
            case 'u': append_utf8(r, read_code_point()); break;
            default: fail("bad escape");
            }
        }
    }

    std::uint32_t read_hex4() {
        if (pos_ + 4 > text_.size())
            fail("short \\u escape");
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9')
                v |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f')
                v |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                v |= static_cast<std::uint32_t>(c - 'A' + 10);
            else
                fail("bad \\u escape");
        }
        return v;
    }

    std::uint32_t read_code_point() {
        const auto high = read_hex4();
        if (high < 0xD800 || high > 0xDBFF)
            return high;
        if (text_.substr(pos_, 2) != "\\u")
            fail("unpaired surrogate");
        pos_ += 2;
        const auto low = read_hex4();
        if (low < 0xDC00 || low > 0xDFFF)
            fail("unpaired surrogate");
        return 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00);
    }

    static void append_utf8(std::string& s, std::uint32_t cp) {
        if (cp < 0x80) {
            s.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            s.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            s.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            s.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            s.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            s.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    void skip_blanks() {
        while (pos_ < text_.size() &&
            (text_[pos_] == ' ' || text_[pos_] == '\t' ||
                text_[pos_] == '\n' || text_[pos_] == '\r'))
            ++pos_;
    }

    bool consume(char c) {
        skip_blanks();
        if (pos_ < text_.size() && text_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    void expect(char c) {
        if (!consume(c))
            fail(std::format("expected '{}'", c));
    }

    [[noreturn]] void fail(std::string_view what) const {
        throw std::runtime_error(std::format(
            "compile_commands.json: {} at byte {}", what, pos_));
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

bool is_blank(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_escapable_in_double_quotes(char c) {
    return c == '"' || c == '\\' || c == '$' || c == '`' || c == '\n';
}

}

std::vector<std::string> split_command(std::string_view command) {
    std::vector<std::string> r;
    std::size_t i = 0;
    const auto n = command.size();
    while (i < n) {
        while (i < n && is_blank(command[i]))
            ++i;
        if (i == n)
            break;
        std::string word;
        while (i < n && !is_blank(command[i])) {
            const char c = command[i++];
            if (c == '\'') {
                while (i < n && command[i] != '\'')
                    word.push_back(command[i++]);
                ++i;
            } else if (c == '"') {
                while (i < n && command[i] != '"') {
                    if (command[i] == '\\' && i + 1 < n &&
                        is_escapable_in_double_quotes(command[i + 1]))
                        ++i;
                    word.push_back(command[i++]);
                }
                ++i;
            } else if (c == '\\' && i < n) {
                word.push_back(command[i++]);
            } else {
                word.push_back(c);
            }
        }
        r.push_back(std::move(word));
    }
    return r;
}

std::vector<types::compile_command> parse_compile_commands(
    std::string_view json) {
    return json_reader(json).read_database();
}

}
