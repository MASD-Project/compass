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
#include "compass.cpp/ops/type_widths.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <catch2/catch_test_macros.hpp>

using namespace compass::cpp::ops;

namespace {

/**
 * @brief Each type @p source defines, as name=fields.
 */
std::vector<std::string> widths(std::string_view source) {
    const auto l = lex(source);
    std::vector<std::string> out;
    for (const auto& w : type_widths(l))
        out.push_back(std::string(w.name) + "=" + std::to_string(w.fields));
    return out;
}

using strings = std::vector<std::string>;

}

TEST_CASE("each declarator of a data member counts", "[ops][type_widths]") {
    CHECK(widths("struct a { int x; int y, z; std::string s{\"q\"}; };") ==
        strings{"a=4"});
}

TEST_CASE("template arguments and initialisers do not split a member",
    "[ops][type_widths]") {
    CHECK(widths("struct a { std::map<int, std::vector<int>> m; "
        "std::function<void(int, int)> f = [](int, int) { return; }; "
        "int b = f(1, 2); unsigned c : 3; };") == strings{"a=4"});
}

TEST_CASE("functions, static members, aliases and friends do not count",
    "[ops][type_widths]") {
    CHECK(widths(R"(class a final : public b<c, d> {
    Q_OBJECT
public:
    explicit a(int x) : x_(x), y_{2} {}
    ~a() override = default;
    a& operator=(const a&) = delete;
    int x() const noexcept { return x_; }
    auto y() -> int;
    static constexpr int z = 3;
    using t = int;
    friend class e;
    template <typename T> void f(T);
signals:
    void changed();
private:
    int x_;
    int y_;
};)") == strings{"a=2"});
}

TEST_CASE("a nested type's members count for it alone",
    "[ops][type_widths]") {
    CHECK(widths("struct a { struct b { int p; int q; }; b first; "
        "struct b* second; class c; enum class e { x, y }; int n; };") ==
        strings{"a=3", "b=2"});
}

TEST_CASE("a declaration that is no definition names no type",
    "[ops][type_widths]") {
    CHECK(widths("struct a; struct ORES_X_EXPORT b; void f(struct stat* s) "
        "{ struct stat t; } template <class T, class U> void g(); "
        "enum class h { i, j };").empty());
}

TEST_CASE("an export macro and a specialisation keep the type's name",
    "[ops][type_widths]") {
    CHECK(widths("class ORES_X_EXPORT a { int x; }; template <> struct "
        "hash<k> { std::size_t operator()(const k&) const; };") ==
        strings{"a=1", "hash=0"});
}
