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
#include "compass.codec/ops/unified_diff.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <span>
#include <vector>

namespace compass::codec::ops {

namespace {

/**
 * @brief How many leading bytes git looks at for a NUL to call a file
 * binary.
 */
constexpr std::size_t binary_probe = 8000;

bool is_binary(const std::optional<std::string_view>& s) {
    return s && s->substr(0, binary_probe).find('\0') != std::string_view::npos;
}

/**
 * @brief The lines of @p s, each with its newline; the last one has none
 * when @p s does not end in one.
 */
std::vector<std::string_view> lines_of(std::string_view s) {
    std::vector<std::string_view> out;
    while (!s.empty()) {
        const auto end = s.find('\n');
        const auto size = end == std::string_view::npos ? s.size() : end + 1;
        out.push_back(s.substr(0, size));
        s.remove_prefix(size);
    }
    return out;
}

using lines = std::span<const std::string_view>;

/**
 * @brief Marks the lines of @p a deleted and of @p b inserted by a
 * shortest edit script, with Myers' linear space refinement: find the
 * middle snake of a shortest path, then solve the two halves around it.
 */
class myers final {
public:
    myers(lines a, lines b)
        : a_(a), b_(b), deleted_(a.size(), 0), inserted_(b.size(), 0) {
        compare(0, a.size(), 0, b.size());
    }

    [[nodiscard]] const std::vector<std::uint8_t>& deleted() const noexcept {
        return deleted_;
    }
    [[nodiscard]] const std::vector<std::uint8_t>& inserted() const noexcept {
        return inserted_;
    }

private:
    struct snake {
        std::size_t x0, y0, x1, y1;
    };

    void compare(std::size_t a_lo, std::size_t a_hi, std::size_t b_lo,
        std::size_t b_hi) {
        while (a_lo < a_hi && b_lo < b_hi && a_[a_lo] == b_[b_lo]) {
            ++a_lo;
            ++b_lo;
        }
        while (a_lo < a_hi && b_lo < b_hi && a_[a_hi - 1] == b_[b_hi - 1]) {
            --a_hi;
            --b_hi;
        }
        if (a_lo == a_hi || b_lo == b_hi) {
            std::fill(deleted_.begin() + static_cast<std::ptrdiff_t>(a_lo),
                deleted_.begin() + static_cast<std::ptrdiff_t>(a_hi), 1);
            std::fill(inserted_.begin() + static_cast<std::ptrdiff_t>(b_lo),
                inserted_.begin() + static_cast<std::ptrdiff_t>(b_hi), 1);
            return;
        }
        const auto s = middle(a_lo, a_hi, b_lo, b_hi);
        compare(a_lo, a_lo + s.x0, b_lo, b_lo + s.y0);
        compare(a_lo + s.x1, a_hi, b_lo + s.y1, b_hi);
    }

    /**
     * @brief The middle snake of the shortest path from the start to the
     * end of the two ranges, in coordinates relative to their starts. Both
     * ranges are non-empty and differ in their first and last lines.
     */
    snake middle(std::size_t a_lo, std::size_t a_hi, std::size_t b_lo,
        std::size_t b_hi) {
        const auto n = static_cast<std::ptrdiff_t>(a_hi - a_lo);
        const auto m = static_cast<std::ptrdiff_t>(b_hi - b_lo);
        const auto equal = [&](std::ptrdiff_t x, std::ptrdiff_t y) {
            return a_[a_lo + static_cast<std::size_t>(x)] ==
                b_[b_lo + static_cast<std::size_t>(y)];
        };
        const auto delta = n - m;
        const bool odd = (delta & 1) != 0;
        const auto most = (n + m + 1) / 2;
        // Forward diagonals run from -most to most and backward ones from
        // delta - most to delta + most, each with one spare at either end.
        const auto width = static_cast<std::size_t>(2 * most + 3);
        forward_.assign(width, 0);
        backward_.assign(width, 0);
        const auto f = [&](std::ptrdiff_t k) -> std::ptrdiff_t& {
            return forward_[static_cast<std::size_t>(k + most + 1)];
        };
        const auto r = [&](std::ptrdiff_t k) -> std::ptrdiff_t& {
            return backward_[static_cast<std::size_t>(k - delta + most + 1)];
        };
        f(1) = 0;
        r(delta - 1) = n;
        for (std::ptrdiff_t d = 0; d <= most; ++d) {
            for (auto k = -d; k <= d; k += 2) {
                auto x = k == -d || (k != d && f(k - 1) < f(k + 1))
                    ? f(k + 1) : f(k - 1) + 1;
                auto y = x - k;
                const auto x0 = x;
                const auto y0 = y;
                while (x < n && y < m && equal(x, y)) {
                    ++x;
                    ++y;
                }
                f(k) = x;
                if (odd && k >= delta - (d - 1) && k <= delta + (d - 1) &&
                    f(k) >= r(k))
                    return {static_cast<std::size_t>(x0),
                        static_cast<std::size_t>(y0),
                        static_cast<std::size_t>(x),
                        static_cast<std::size_t>(y)};
            }
            for (auto k = delta - d; k <= delta + d; k += 2) {
                auto x = k == delta + d ||
                        (k != delta - d && r(k - 1) <= r(k + 1) - 1)
                    ? r(k - 1) : r(k + 1) - 1;
                auto y = x - k;
                const auto x1 = x;
                const auto y1 = y;
                while (x > 0 && y > 0 && equal(x - 1, y - 1)) {
                    --x;
                    --y;
                }
                r(k) = x;
                if (!odd && k >= -d && k <= d && r(k) <= f(k))
                    return {static_cast<std::size_t>(x),
                        static_cast<std::size_t>(y),
                        static_cast<std::size_t>(x1),
                        static_cast<std::size_t>(y1)};
            }
        }
        return {0, 0, 0, 0};
    }

    lines a_;
    lines b_;
    std::vector<std::uint8_t> deleted_;
    std::vector<std::uint8_t> inserted_;
    std::vector<std::ptrdiff_t> forward_;
    std::vector<std::ptrdiff_t> backward_;
};

/**
 * @brief One line of the diff: kept, deleted or inserted, with the
 * position it takes in either file.
 */
struct edit {
    char kind;
    std::size_t a;
    std::size_t b;
};

std::vector<edit> edits_of(lines a, lines b) {
    const myers m(a, b);
    std::vector<edit> out;
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < a.size() || j < b.size()) {
        if (i < a.size() && m.deleted()[i] != 0)
            out.push_back({'-', i++, j});
        else if (j < b.size() && m.inserted()[j] != 0)
            out.push_back({'+', i, j++});
        else
            out.push_back({' ', i++, j++});
    }
    return out;
}

/**
 * @brief A hunk header range: the first line, or the one before it when
 * the range is empty, and the count unless it is one.
 */
std::string range(std::size_t start, std::size_t count) {
    const auto first = count == 0 ? start : start + 1;
    return count == 1 ? std::format("{}", first)
                      : std::format("{},{}", first, count);
}

void append_line(std::string& out, char kind, std::string_view line) {
    out += kind;
    out += line;
    if (line.empty() || line.back() != '\n')
        out += "\n\\ No newline at end of file\n";
}

}

std::string unified_diff(std::string_view path,
    const std::optional<std::string_view>& before,
    const std::optional<std::string_view>& after, std::size_t context) {
    if (before.has_value() == after.has_value() &&
        before.value_or("") == after.value_or(""))
        return {};
    const auto old_name = before ? std::format("a/{}", path) : "/dev/null";
    const auto new_name = after ? std::format("b/{}", path) : "/dev/null";
    if (is_binary(before) || is_binary(after))
        return std::format("Binary files {} and {} differ\n", old_name,
            new_name);

    auto out = std::format("--- {}\n+++ {}\n", old_name, new_name);
    const auto a = lines_of(before.value_or(""));
    const auto b = lines_of(after.value_or(""));
    const auto edits = edits_of(a, b);
    std::vector<std::size_t> changes;
    for (std::size_t e = 0; e < edits.size(); ++e) {
        if (edits[e].kind != ' ')
            changes.push_back(e);
    }
    for (std::size_t c = 0; c < changes.size();) {
        auto last = c;
        while (last + 1 < changes.size() &&
            changes[last + 1] - changes[last] - 1 <= 2 * context)
            ++last;
        const auto begin = changes[c] - std::min(changes[c], context);
        const auto end = std::min(edits.size(), changes[last] + context + 1);
        std::size_t old_count = 0;
        std::size_t new_count = 0;
        for (auto e = begin; e < end; ++e) {
            old_count += edits[e].kind != '+' ? 1 : 0;
            new_count += edits[e].kind != '-' ? 1 : 0;
        }
        out += std::format("@@ -{} +{} @@\n", range(edits[begin].a, old_count),
            range(edits[begin].b, new_count));
        for (auto e = begin; e < end; ++e) {
            const auto& x = edits[e];
            append_line(out, x.kind, x.kind == '+' ? b[x.b] : a[x.a]);
        }
        c = last + 1;
    }
    return out;
}

}
